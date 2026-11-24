#include "Icons.h"

namespace draw {

void icon(Canvas& canvas, Icon icon, int x, int y, IconSize size) {
  u8g2_t* g = canvas.g();
  int s = static_cast<int>(size);
  u8g2_SetBitmapMode(g, 1);  // transparent: white icon pixels leave what is underneath alone
  u8g2_DrawXBM(g, x, y, s, s, icons::bits(icon, size));
  u8g2_SetBitmapMode(g, 0);
}

}  // namespace draw

Icon iconForWmo(uint8_t code, bool isDay) {
  switch (code) {
    case 0:
      return isDay ? Icon::ClearDay : Icon::ClearNight;
    case 1:
    case 2:
      return isDay ? Icon::PartlyCloudyDay : Icon::PartlyCloudyNight;
    case 3:
      return Icon::Cloudy;
    case 45:
    case 48:
      return Icon::Fog;
    default:
      break;
  }
  if (code >= 51 && code <= 57) return Icon::Drizzle;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Icon::Rain;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Icon::Snow;
  if (code >= 95 && code <= 99) return Icon::Thunder;
  return Icon::Unknown;
}
