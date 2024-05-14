#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
  printf( "ONE\n" );
  fprintf( stderr, "ERROR: ONE\n" );
  int rc = close( 2 );
  printf( "==> %d\n", rc );

  printf( "TWO\n" );
  fprintf( stderr, "ERROR: TWO\n" );
  rc = dup2( 1, 2 );
  printf( "==> %d\n", rc );

  printf( "THREE\n" );
  fprintf( stderr, "ERROR: THREE\n" );

  return EXIT_SUCCESS;
}


