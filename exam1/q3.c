#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
  char * a = "POLYTECHNIC";
  char * b = a;
  char * c = calloc( 100, sizeof( char ) );

  printf( "[%s][%s][%s]\n", a + 10, b + 9, c + 8 );

  char ** d = calloc( 100, sizeof( char * ) );
  d[7] = calloc( 20, sizeof( char ) );
  d[6] = c;
  strcpy( d[7], b + 5 );
  strcpy( d[6], b + 4 );

  printf( "[%s][%s][%s]\n", d[7], d[6], c + 5 );

  float e = 2.71828;
  float * f = calloc( 1, sizeof( float ) );
  float * g = f;
  float * h = &e;

  printf( "[%3.2f][%2.2f][%2.1f]\n", *f, *g, *h );

  return EXIT_SUCCESS;
}
