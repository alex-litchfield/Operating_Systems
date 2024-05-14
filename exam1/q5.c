#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main()
{
  int fd;

  close( 2 );

  printf( "HI\n" );

#if 0
  close( 1 );  /* <== add this line later.... */
#endif

  fd = open( "output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600 );

  printf( "==> %d\n", fd );
  printf( "WHAT?\n" );
  fprintf( stderr, "ERROR\n" );

  close( fd );

  return EXIT_SUCCESS;
}


