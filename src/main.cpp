#include "app.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCmd) {
  App app;
  return app.Run(instance, showCmd);
}
