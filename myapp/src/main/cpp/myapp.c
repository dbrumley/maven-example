#include "myapp.h"
#include <stdio.h>

void conversion(celsius_t c, kelvin_t k, char *buf)
{
  sprintf(buf, "%lf celsius is %lf kelvin", c, k);
  return;
}
