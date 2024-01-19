#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "temperature.h"

int main(int argc, char *argv[])
{
  char buf[256];
  double c = atof(argv[1]);
  double k = CelsiusToFahrenheit(c);
  conversion(c, k, buf);

  printf("Running baselib code CelsiusToFahrenheit(%lf): %lf\n", c, k);
  printf("kelvinToString: %s\n", buf);
  return 1;
}