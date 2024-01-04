#include <unistd.h>
#include <fcntl.h>

#include "applib.h"

int main(int argc, char *argv[])
{
  int fd;
  char buf[16];
  double c = 0.0;
  if ((fd = open(argv[1], O_RDONLY)) < 0)
  {
    return -1;
  }

  read(fd, &c, sizeof(c));
  double k = CelsiusToFahrenheit(c);
  conversion(c, k, buf);
  printf("%s\n", buf);
  return 0;
}