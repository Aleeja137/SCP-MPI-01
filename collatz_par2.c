/*  SCP, Sistemas de Cómputo Paralelo -- GII (Ingeniería de Computadores)
    Laboratorio de MPI

    collatz_ser.c

***************************************************************************/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <mpi.h>

#define ZMAX 1000

void Crear_Tipo_Resultado(int *tarea_id, int *iter_count, MPI_Datatype *PARAM)
{
  int           tam[2];
  MPI_Aint      dist[2], dir1, dir2;
  MPI_Datatype  tipo[2];

  tam[0] = tam[1] = 1;

  tipo[0] = tipo[1] = MPI_INT;

  dist[0] = 0;
  MPI_Get_address(tarea_id,&dir1);
  MPI_Get_address(iter_count,&dir2);
  dist[1] = dir2 - dir1;

  MPI_Type_create_struct (2, tam, dist, tipo, PARAM);
  MPI_Type_commit (PARAM);
}

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
  // Variables para cálculo
  int  n = 1, iter, num_iter = 0, iter_max = 0, z_max;
  struct timespec  t0, t1;
  double  tej;

  // Variables para MPI
  int pid, npr, tarea, origen, destino, tag, ult_tarea, proc_acabados = 1, i, acabado = 0;
  MPI_Status info;

  // ---
  int suma_tareas = 0;

  // Vector que guarda número de tareas y número de iteraciones de cada worker
  // Para info_procesos[0] guarda la tarea con más iteraciones y el número de iteraciones
  struct worker_info{ 
        int num_tareas; 
        int num_iteraciones; 
  } *info_procesos; 

  struct resultado_t {
    int tarea_id;
    int iter_count;
  } resultado;

  MPI_Datatype resultado_type;

  MPI_Init (&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &pid);
  MPI_Comm_size (MPI_COMM_WORLD, &npr);

  // Inicializar estructuras
    info_procesos = (struct worker_info*) malloc (npr*sizeof(struct worker_info));
    Crear_Tipo_Resultado(&resultado.tarea_id,&resultado.iter_count,&resultado_type);
    resultado.tarea_id = 0; resultado.iter_count = 0;
    for (i=0;i<npr;i++)
    {
      info_procesos[i].num_tareas = 0;
      info_procesos[0].num_iteraciones = 0;
    }

  if (pid == 0)
  {
    printf ("\n COLLATZ (serie): 1 - %d  -- calculando", ZMAX);
    printf ("\n =======================================\n\n");

    clock_gettime (CLOCK_REALTIME, &t0);

    while (proc_acabados < npr)
    {
      // Solicitudes, primera o el resto

      MPI_Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&info);

      origen = info.MPI_SOURCE;
      tag = info.MPI_TAG;

      switch (tag)
      {
        // Petición inicial de la tarea, recibir mensaje vacío
        case 0:
          MPI_Recv(&tarea,0,MPI_INT,origen,tag,MPI_COMM_WORLD,&info);
          //printf("Recibida petición inicial de P%d\n",origen);
          break;
        
        // Tarea finalizada, recibir resultados
        case 2:
          MPI_Recv(&resultado,1,resultado_type,origen,tag,MPI_COMM_WORLD,&info);
          //printf("Recibido resultado de P%d\n",origen);
          // Se añade 1 a las tareas del worker y se acumulan sus iteraciones
          info_procesos[origen].num_tareas++;
          info_procesos[origen].num_iteraciones += resultado.iter_count;
          // Si es necesario, se actualiza la tarea con más iteraciones
          if (resultado.iter_count > info_procesos[0].num_iteraciones) 
          {
            info_procesos[0].num_tareas = resultado.tarea_id;
            info_procesos[0].num_iteraciones = resultado.iter_count;
          }
          break;

        default:
          break;
      }
    
      destino = origen;

      // Si hay tareas, mandar tarea con tag 1
      if (n <= ZMAX)
      {
        tag = 1;
        MPI_Send(&n,1,MPI_INT,destino,tag,MPI_COMM_WORLD);
        //printf("Mandada tarea %d a P%d\n",n,destino);
        n++;
      }
      // Si no hay tareas, sumar uno a los procesos acabados y mandar mensaje con tag 3 indicando que no hay más tareas
      else
      {
        tag = 3;
        MPI_Send(&pid,0,MPI_INT,destino,tag,MPI_COMM_WORLD);
        //printf("Mandado mensaje de final a P%d\n",destino);
        proc_acabados++;
      }
    }
    
    // Fuera del while, imprimir resultados
    clock_gettime (CLOCK_REALTIME, &t1);
    tej = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / (double)1e9;
    printf("------------------------------------------------------\n\n");
    for (i=1;i<npr;i++) printf(" P%d ha hecho %d tareas con un total de %d iteraciones \n", i,info_procesos[i].num_tareas,info_procesos[i].num_iteraciones);
    for (i=1;i<npr;i++) num_iter += info_procesos[i].num_iteraciones;
    printf ("\n  >> Total: %d iteraciones\n  >> Z_max:   %3d (%3d iteraciones)\n\n  >> Tej:     %1.3f ms\n\n", 
                    num_iter, info_procesos[0].num_tareas, info_procesos[0].num_iteraciones, tej*1000);

  } 
  else 
  {
    destino = 0;
    // Si los resultados son cero, significa que es la primera petición
    if (resultado.tarea_id == 0)
    {
      tag = 0;
      MPI_Send(&pid,0,MPI_INT,destino,tag,MPI_COMM_WORLD);
    } 

    while (acabado == 0)
    {
      MPI_Probe(MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&info);

      origen = info.MPI_SOURCE;
      tag = info.MPI_TAG;
      switch (tag)
      {
        case 1:

          // Recibir la tarea
          MPI_Recv(&resultado.tarea_id,1,MPI_INT,origen,tag,MPI_COMM_WORLD,&info);

          // Hacer el cálculo de la tarea
          resultado.iter_count = collatz (resultado.tarea_id);
          carga (resultado.iter_count);

          // Mandar resultado
          tag = 2;
          //printf("Soy P%d con tarea %d y he necesitado %d iteraciones \n",pid,resultado.tarea_id,resultado.iter_count);
          MPI_Send(&resultado,1,resultado_type,destino,tag,MPI_COMM_WORLD);

          break;
        case 3:
          MPI_Recv(&pid,0,MPI_INT,origen,tag,MPI_COMM_WORLD,&info);
          acabado = 1;
          break;
        default:
          break;
      }
    }
  }

  // Los frees (PENDIENTE)
  MPI_Finalize ();
  return (0); 
}

