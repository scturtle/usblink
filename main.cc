#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <switch.h>

#include "usb.h"

const u32 MAGIC = 0x54555452; // "TUTR"
const u32 MAXSIZE = 0x1000000;

struct UsblinkHeader {
  u32 magic = MAGIC;
  u32 type = 0;
  u32 start = 0;
  u32 end = 0;
};
static_assert(sizeof(UsblinkHeader) == 16, "");

alignas(0x1000) static UsblinkHeader req;
alignas(0x1000) static UsblinkHeader resp;
alignas(0x1000) static char fn[FS_MAX_PATH];
alignas(0x1000) static char buf[MAXSIZE];

class UsbLinkStateMachine {

  enum TypeCode {
    TYPE_UNINIT = 0,
    TYPE_FILENAME = 1,
    TYPE_FILERANGE = 2,
    TYPE_ACK = 3,
  };

  enum Status {
    WAIT_CONNECT = 0,
    WAIT_FILENAME = 1,
    WAIT_FILERANGE = 2,
    WAIT_FILEDATA = 3,
  };

  Status status = WAIT_CONNECT;
  int fn_len = 0;
  int pos = 0;
  FILE *fp = nullptr;

  bool check(TypeCode type) {
    if (req.magic != MAGIC) {
      printf("magic error\n");
      return false;
    }
    if (req.type != type) {
      printf("expect type %d but got %d\n", type, req.type);
      return false;
    }
    return true;
  }

  void ack() {
    resp.type = 3;
    custom_usbCommsWrite(&resp, sizeof(resp));
  }

public:
  void step() {
    bool connected = R_SUCCEEDED(usbDsWaitReady(1'000'000));
    if (status != WAIT_CONNECT && !connected) {
      printf("disconnected\n");
      if (fp) {
        fclose(fp);
        fp = nullptr;
      }
      status = WAIT_CONNECT;
      return;
    }
    if (status == WAIT_CONNECT) {
      if (connected) {
        printf("connected\n");
        status = WAIT_FILENAME;
      }
    } else if (status == WAIT_FILENAME) {
      size_t read = custom_usbCommsRead(&req, sizeof(req), 1'000'000);
      if (read == 0)
        return; // wait again
      ack();
      if (read < sizeof(UsblinkHeader) || !check(TYPE_FILENAME)) {
        printf("filename header error\n");
      } else {
        fn_len = req.end;
        custom_usbCommsRead(fn, fn_len);
        ack();
        fn[fn_len] = 0;
        printf("start %s\n", fn);
        pos = 0;
        fp = fopen(fn, "w");
        status = WAIT_FILERANGE;
      }
    } else if (status == WAIT_FILERANGE) {
      size_t read = custom_usbCommsRead(&req, sizeof(req), 1'000'000);
      if (read == 0) {
        return;
      }
      ack();
      if (read < sizeof(UsblinkHeader) || !check(TYPE_FILERANGE)) {
        printf("range header error\n");
        fclose(fp);
        fp = nullptr;
        status = WAIT_FILENAME;
      } else if (req.start == req.end) { // done
        fclose(fp);
        fp = nullptr;
        printf("saved %s\n", fn);
        status = WAIT_FILENAME;
      } else if (req.start != pos) { // range error
        fclose(fp);
        fp = nullptr;
        printf("range error: start(%u) != pos(%u)\n", req.start, pos);
        status = WAIT_FILENAME;
      } else {
        status = WAIT_FILEDATA;
      }
    } else if (status == WAIT_FILEDATA) {
      u64 timeout = 10'000'000'000ull;
      size_t read = custom_usbCommsRead(buf, req.end - req.start, timeout);
      if (read == 0)
        return;
      ack();
      if (read != req.end - req.start) {
        fclose(fp);
        fp = nullptr;
        printf("recv range %u-%u error\n", req.start, req.end);
        status = WAIT_FILENAME;
      } else {
        fwrite(buf, 1, req.end - req.start, fp);
        pos += req.end - req.start;
        printf("recv range %u-%u ok\n", req.start, req.end);
        status = WAIT_FILERANGE;
      }
    }
  }
};

int main(int argc, char **argv) {
  consoleInit(nullptr);
  custom_usbCommsInitialize();

  PadState pad;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);

  printf("hello usblink\n");
  UsbLinkStateMachine usblink;

  while (appletMainLoop()) {
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    if (kDown & HidNpadButton_Plus)
      break;

    usblink.step();

    consoleUpdate(NULL);
  }

  custom_usbCommsExit();
  consoleExit(nullptr);
  exit(0);
}
