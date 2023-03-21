/*  SCP, Sistemas de Cómputo Paralelo -- GII (Ingeniería de Computadores)
    Laboratorio de MPI

    collatz_p1.c

***************************************************************************/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <mpi.h>

#define ZMAX 1000


int collatz (int n)
{
  int  iter = 0;

  while (n > 1)
  {
    if ((n%2) == 1) n = 3*n + 1;
    else            n = n/2;
 
    iter++;
  }
  return (iter);
}


void carga (int iter)
{
  usleep (1000*iter);
}


int main (int argc, char *argv[])
{
  // Variables funcionamiento
  int  n, iter, num_iter = 0;
  int  num_iter_global = 0;
  struct timespec  t0, t1;
  double  tej;

  // Variables MPI
  int     i, pid, npr, cociente, resto, *tam, *dis;
  MPI_Status info;
  struct { 
        int iter_max; 
        int z_max; 
    } local_pair, global_pair;

  local_pair.iter_max = 0;

  MPI_Init (&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &pid);
  MPI_Comm_size (MPI_COMM_WORLD, &npr);

  // Reparto de tareas estático consecutivo con resto acumulado
  // PREGUNTA (¿Es mejor que lo calcule uno y broadcast? ¿O mejor lo calculan todos?)
  tam = (int *) malloc (npr*sizeof(int));
  dis = (int *) malloc (npr*sizeof(int));

  cociente = ZMAX/npr;
  resto = ZMAX % npr;

  for (i=0;i<npr;i++){
    tam[i] = (i+1)*cociente - i*cociente ;
    if (i == 0) dis[i] = 0;
    else dis[i] = tam[i-1] + dis[i-1];
  }
  
  // P0 imprime cabecera y toma el tiempo de start
  if (pid == 0){
    printf ("\n COLLATZ (serie): 1 - %d  -- calculando", ZMAX);
    printf ("\n =======================================\n\n");

    clock_gettime (CLOCK_REALTIME, &t0);
  }  

  // Cada procesador se encarga de sus tareas
  for (n=dis[pid]; n<=dis[pid]+tam[pid]; n++)
  {
    iter = collatz (n);
    carga (iter);

    num_iter += iter;
    if (iter > local_pair.iter_max) {
      local_pair.z_max = n; 
      local_pair.iter_max = iter;
    }
  }

  // Cada procesador envía sus resultados a P0 acumulándolos o cogiendo el máximo
  // PREGUNTA (¿Merece la pena empaquetar o struct?)

  MPI_Reduce(&num_iter,&num_iter_global,1,MPI_INT,MPI_SUM,0,MPI_COMM_WORLD);  
  MPI_Reduce(&local_pair,&global_pair,1,MPI_2INT,MPI_MAXLOC,0,MPI_COMM_WORLD);

  // Cada procesador imprime sus resultados parciales
  printf ("\n  >> P%d parcial: %d iteraciones  >> Z_max:   %3d (%3d iteraciones) \n", 
                  pid, num_iter, local_pair.z_max, local_pair.iter_max);
 
  // P0 coge la medida de tiempo de finish e imprime los resultados totales
  if (pid == 0){
    clock_gettime (CLOCK_REALTIME, &t1);
    tej = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / (double)1e9;
    printf ("\n\n  >> Total: %d iteraciones\n  >> Z_max:   %3d (%3d iteraciones)\n\n  >> Tej:     %1.3f ms\n\n", 
                  num_iter_global, global_pair.z_max, global_pair.iter_max, tej*1000);
  }
  
  MPI_Finalize ();
  free (tam);
  free(dis);
  return (0); 
}

