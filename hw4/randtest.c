/* hw4.c */

/* TO DO: free() */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main()
{
  srand(111);
  int w = rand() % 5757;
  printf("%d\n",w);
  return EXIT_SUCCESS;
}
