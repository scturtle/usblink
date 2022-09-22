#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <switch.h>

#include "usb.h"

PadState pad;

const u32 MAGIC = 0x54555452; // "TUTR"
const u32 MAXSIZE = 0x1000000;

struct Header {
  u32 magic = MAGIC;
  u32 type = 0;
  u32 start = 0;
  u32 end = 0;
};
static_assert(sizeof(Header) == 16, "");

alignas(0x1000) Header req;
alignas(0x1000) Header resp;
alignas(0x1000) char fn[FS_MAX_PATH];
alignas(0x1000) char buf[MAXSIZE];

int check(int typ) {
  if (req.magic != MAGIC) {
    printf("magic error\n");
    return 1;
  }
  if (req.type != typ) {
    printf("expect type %d but got %d\n", typ, req.type);
    return 1;
  }
  return 0;
}

void ack() {
  resp.type = 3;
  custom_usbCommsWrite(&resp, sizeof(resp));
}

int usblink_server() {
  // wait for filename length
  size_t read = custom_usbCommsRead(&req, sizeof(req), 1'000'000);
  if (read == 0)
    return 0;
  if (read < sizeof(req)) {
    printf("header size error\n");
    return 1;
  }
  if (check(/*req.type=*/1))
    return 1;
  ack();
  int fn_len = req.end;
  custom_usbCommsRead(fn, fn_len);
  ack();
  fn[fn_len] = 0;
  printf("start %s\n", fn);
  u32 pos = 0;
  FILE *fp = fopen(fn, "w");
  while (1) {
    custom_usbCommsRead(&req, sizeof(req));
    ack();
    if (check(/*req.type=*/2))
      return 1;
    if (req.start == req.end)
      break;
    if (req.start != pos) {
      printf("start(%u) != curr pos(%u)\n", req.start, pos);
      return 1;
    }
    custom_usbCommsRead(buf, req.end - req.start);
    ack();
    fwrite(buf, 1, req.end - req.start, fp);
    pos += req.end - req.start;
    printf("recv range %u-%u ok\n", req.start, req.end);
  }
  fclose(fp);
  printf("saved %s\n", fn);
  return 0;
}

int main(int argc, char **argv) {
  consoleInit(nullptr);
  custom_usbCommsInitialize();

  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);

  printf("usblink server started\n");

  while (appletMainLoop()) {
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    if (kDown & HidNpadButton_Plus)
      break;

    if (usblink_server())
      break;

    consoleUpdate(NULL);
  }

  custom_usbCommsExit();
  consoleExit(nullptr);
  exit(0);
}
