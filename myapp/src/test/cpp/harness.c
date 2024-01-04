#include "myapp.h"

int main(int argc, char *argv[])
{
  char buf[256];
  double c = atof(argv[1]);
  double k = CelsiusToFahrenheit(c);
  conversion(c, k, buf);

  return 1;
}