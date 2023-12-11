/**************************************************************************
ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
// reassembled in one file for CSC 412/512 project
#define PRId64 "ld" // does not get defined by headers/inttypes because windows version got installed by SPEC
//#include "mcf/mcf.c"
/**************************************************************************
MCF.H of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Wed May 26 00:06:03 2010 by Loebel (opt0.zib.de)  */
/*  $Id: mcf.c,v 1.17 2010/05/26 08:26:29 bzfloebe Exp $  */
#include "mcf/mcf.h"
#include "time.h"
#define REPORT
extern LONG min_impl_duration;
network_t net;
#ifdef _PROTO_
LONG global_opt( void )
#else
LONG global_opt( )
#endif
{
  LONG new_arcs;
  LONG residual_nb_it = 1;
  new_arcs = -1;
  while( new_arcs )
  {
#ifdef REPORT
    printf( "active arcs                : %" PRId64 "\n", net.m );
#endif
    primal_net_simplex( &net );
#ifdef REPORT
    printf( "simplex iterations         : %" PRId64 "\n", net.iterations );
    printf( "objective value            : %0.0f\n", flow_cost(&net) );
#endif
#if defined AT_HOME
    printf( "iterations                 : %ld\n", residual_nb_it );
#endif
    if( net.m_impl )
    {
      new_arcs = suspend_impl( &net, (cost_t)-1, 0 );
      if ( new_arcs < 0)
      {
#ifdef REPORT
        printf( "not enough memory, exit(-1)\n" );
#endif
        exit(-1);
      }
#ifdef REPORT
      if( new_arcs )
        printf( "erased arcs                : %" PRId64 "\n", new_arcs );
#endif
    }
    else 
    {
      refreshPositions(&net, &getOriginalArcPosition,net.m);
    }
    new_arcs = price_out_impl( &net );
#ifdef REPORT
    if( new_arcs )
      printf( "new implicit arcs          : %" PRId64 "\n", new_arcs );
#endif
    if( new_arcs < 0 )
    {
#ifdef REPORT
      printf( "not enough memory, exit(-1)\n" );
#endif
      exit(-1);
    }
#ifndef REPORT
    printf( "\n" );
#endif
    residual_nb_it++;
  }
  net.optcost = flow_cost(&net);
  printf( "checksum                   : %0.0f\n", net.optcost );
  return 0;
}
#ifdef _PROTO_
int main( int argc, char *argv[] )
#else
int main( argc, argv )
    int argc;
    char *argv[];
#endif
{
  int outnum; 
  char outfile[80];
  if( argc < 2 )
    return -1;
#ifndef SPEC
  time_t startTime, endTime;
  time(&startTime);
#endif
  printf( "\nMCF SPEC CPU version 1.11\n" );
  printf( "Copyright (c) 1998-2000 Zuse Institut Berlin (ZIB)\n" );
  printf( "Copyright (c) 2000-2002 Andreas Loebel & ZIB\n" );
  printf( "Copyright (c) 2003-2005 Andreas Loebel\n" );
  printf( "Copyright (c) 2006-2010 Dres. Loebel, Borndoerfer & Weider "\
          "GbR (LBW)\n" );
  printf( "\n" );
  //omp_set_num_threads(1);
  memset( (void *)(&net), 0, (size_t)sizeof(network_t) );
  net.bigM = (LONG)BIGM;
  strcpy( net.inputfile, argv[1] );
  if (argc == 3) {
     outnum = atoi(argv[2]);
     sprintf(outfile,"mcf.%d.out",outnum);
  } else {
     strcpy(outfile,"mcf.out"); 
  }  
  if( read_min( &net ) )
  {
    printf( "read error, exit\n" );
    getfree( &net );
    return -1;
  }
#ifndef SPEC
#ifdef _OPENMP
  printf( "number of threads          : %d\n", omp_get_max_threads() );
#else
  printf( "single threaded\n" );
#endif
#endif
#if defined(REPORT) || defined(SPEC)
  printf( "nodes                      : %" PRId64 "\n", net.n_trips );
#endif
  primal_start_artificial( &net );
  global_opt( );
  if( write_objective_value( outfile, &net ) )
  {
    getfree( &net );
    return -1;    
  }
  getfree( &net );
#ifndef SPEC
  time(&endTime);
  printf("runtime = %ld seconds\n",endTime - startTime);
#endif
#ifdef REPORT
  printf( "done\n" );
#endif
  return 0;
}
//#include "mcf/implicit.c"
/**************************************************************************
IMPLICIT.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:46:30 2010 by Loebel (opt0.zib.de)  */
/*  $Id: implicit.c,v 1.21 2010/05/25 21:58:44 bzfloebe Exp $  */
#if defined(SPEC)
# include "mcf/spec_qsort.h"
#endif
#include "mcf/implicit.h"
#ifdef _PROTO_
static int arc_compare( arc_t **a1, arc_t **a2 )
#else
static int arc_compare( a1, a2 )
arc_t **a1;
arc_t **a2;
#endif
{
  if( (*a1)->flow > (*a2)->flow )
    return 1;
  if( (*a1)->flow < (*a2)->flow )
    return -1;
  if( (*a1)->id < (*a2)->id )
    return -1;
    return 1;
}
#ifdef _PROTO_
LONG refreshArcPointers(network_t *net, LONG (*getPos)(network_t *, LONG), arc_t* sorted_array)
#else
refreshArcPointers(net, getPos, sorted_array)
network_t *net;
arc_t* sorted_array;
LONG (*getPos)(network_t *, LONG);
#endif
{
  node_p node;
  LONG i;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for private(node)
#endif
    for (i = 0; i <= net->n; i++) {
      node = net->nodes + i;
      if (node->basic_arc && node->basic_arc->id >= 0)
          node->basic_arc = &sorted_array[getPos(net, node->basic_arc->id)];
      if (node->firstin && node->firstin->id >= 0)
          node->firstin = &sorted_array[getPos(net, node->firstin->id)];
      if (node->firstout && node->firstout->id >= 0)
          node->firstout = &sorted_array[getPos(net, node->firstout->id)];
    }
    return 0;
}
#ifdef _PROTO_
LONG refreshPositions( network_t *net, LONG (*getPos)(network_t *, LONG),LONG new_m)
#else
LONG refreshPositions( net, getPos, new_m)
network_t *net;
LONG (*getPos)(LONG);
LONG new_m;
#endif
{
  arc_t *arc, *sorted_array;
    LONG position, new_position;
    sorted_array = net->sorted_arcs;
    refreshArcPointers(net, getPos, sorted_array);
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for private(arc, new_position)
#endif
  for (position = 0; position < new_m; position++) {
    arc = net->arcs + position;
    if(arc->id < 0)
      continue;
    new_position = getPos(net, arc->id);
    sorted_array[new_position] = *arc;
  }
  arc = net->arcs;
  net->arcs = sorted_array;
  net->sorted_arcs = arc;
  net->stop_arcs = sorted_array + new_m;
  return 0;
}
#ifdef _PROTO_
void marc_arcs(network_t* net, LONG *new_arcs, LONG *new_arcs_array, arc_p **arcs_pointer_sorted)
#else
void marc_arcs(net, new_arcs, new_arcs_array, arcs_pointer_sorted)
network_t* net;
LONG *new_arcs;
LONG *new_arcs_array;
arc_p **arcs_pointer_sorted;
#endif
{
  LONG max_new_arcs;
  arc_p **positions;
  LONG *values;
  LONG global_new = 0;
  LONG best_pos = 0;
  LONG start_id, i;
  arc_t* arc;
  #if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
  LONG num_threads = omp_get_max_threads();
  #else
  LONG num_threads = 1;
  #endif
  positions = (arc_p**) malloc(num_threads * sizeof(arc_p*));
  values = (LONG*) malloc(num_threads * sizeof(LONG));
  if (net->n_trips <= MAX_NB_TRIPS_FOR_SMALL_NET)
    max_new_arcs = net->max_residual_new_m - MAX_NEW_ARCS_PUFFER_SMALL_NET;
  else
    max_new_arcs = net->max_residual_new_m - MAX_NEW_ARCS_PUFFER_LARGE_NET;
  *new_arcs = 0;
  for(i = 0; i< num_threads; i++) {
    *new_arcs += new_arcs_array[i];
    positions[i] = arcs_pointer_sorted[i];
    values[i] = 0;
  }
  start_id = net->m;
  while(global_new < *new_arcs && global_new < max_new_arcs) {
    if (values[0] < new_arcs_array[0]) {
            arc = *positions[0];
            best_pos = 0;
    }
    else {
      arc = 0;
    }
        for (i = 1; i < num_threads; i++) {
            if ((values[i] < new_arcs_array[i]) && ((!arc) || arc_compare(positions[i], &arc) < 0)) {
                arc = *positions[i];
                best_pos = i;
            }
        }
//        if (global_new >= 3999000)
//            printf("global_new %ld redcost %ld arcid %d  \n",global_new,arc->flow, arc->id);
        arc->id = start_id++;
        arc->flow = 1;
        global_new++;
        positions[best_pos]++;
        values[best_pos]++;
  }
//    arc_t* nu;
//    nu->id = 0;
  *new_arcs = 0;
  for (i = 0; i< num_threads; i++) {
    *new_arcs += values[i];
  }
  net->max_elems = K;
  net->nr_group = ( (*new_arcs + net->m -1) / K ) + 1;
  if ((*new_arcs + net->m) % K != 0)
      net->full_groups = net->nr_group - (K - ((*new_arcs + net->m) % K));
  else
      net->full_groups = net->nr_group;
  while (net->full_groups < 0) {
    net->full_groups = net->nr_group + net->full_groups;
    net->max_elems--;
  }
    free(positions);
    free(values);
}
#ifdef _PROTO_
LONG resize_prob( network_t *net )
#else
LONG resize_prob( net )
     network_t *net;
#endif
{
    arc_t *arc, *old_arcs;
    node_t *node, *stop, *root;
    size_t off;
    assert( net->max_new_m >= 3 );
    net->max_m += net->max_new_m;
    net->max_residual_new_m += net->max_new_m;
#if defined AT_HOME
    printf( "\nresize arcs to %4ld MB (%ld elements a %lu byte)\n\n",
            net->max_m * sizeof(arc_t) / 0x100000,
            net->max_m,
            (unsigned LONG)sizeof(arc_t) );
    fflush( stdout );
#endif
    arc = (arc_t *) realloc( net->arcs, net->max_m * sizeof(arc_t) );
    if( !arc )
    {
        printf( "network %s: not enough memory\n", net->inputfile );
        fflush( stdout );
        return -1;
    }
    old_arcs = net->arcs;
    net->arcs = arc;
    net->stop_arcs = arc + net->m;
    root = node = net->nodes;
    for(node++, stop = net->stop_nodes; node < stop; node++ ) {
       if( node->basic_arc && node->pred != root) {
          off = node->basic_arc - old_arcs;
            node->basic_arc = (arc_t *)(net->arcs + off);
        }
    }
    arc = (arc_t *) realloc( net->sorted_arcs, net->max_m * sizeof(arc_t) );
    net->sorted_arcs = arc;
    return 0;
}
#ifdef _PROTO_
void insert_new_arc(arc_t *newarc, LONG newpos, node_t *tail, node_t *head,
    cost_t cost, cost_t red_cost, LONG m, LONG number)
#else
void insert_new_arc( newarc, newpos, tail, head, cost, red_cost, m, number)
     arc_t *newarc;
     LONG newpos;
     node_t *tail;
     node_t *head;
     cost_t cost;
     cost_t red_cost;
     LONG m;
     LONG number;
#endif
{
    LONG pos;
    newarc[newpos].tail      = tail;
    newarc[newpos].head      = head;
    newarc[newpos].org_cost  = cost;
    newarc[newpos].cost      = cost;
    newarc[newpos].flow      = (flow_t)red_cost;
    newarc[newpos].id        = number;
    pos = newpos+1;
    while( pos-1 && red_cost > (cost_t)newarc[pos/2-1].flow )
    {
        newarc[pos-1].tail     = newarc[pos/2-1].tail;
        newarc[pos-1].head     = newarc[pos/2-1].head;
        newarc[pos-1].cost     = newarc[pos/2-1].cost;
        newarc[pos-1].org_cost = newarc[pos/2-1].cost;
        newarc[pos-1].flow     = newarc[pos/2-1].flow;
        newarc[pos-1].id       = newarc[pos/2-1].id;
        pos = pos/2;
        newarc[pos-1].tail     = tail;
        newarc[pos-1].head     = head;
        newarc[pos-1].cost     = cost;
        newarc[pos-1].org_cost = cost;
        newarc[pos-1].flow     = (flow_t)red_cost;
        newarc[pos-1].id       = number;
    }
    return;
}   
#ifdef _PROTO_
void replace_weaker_arc( arc_t *newarc, node_t *tail, node_t *head,
                         cost_t cost, cost_t red_cost,LONG max_new_par_residual_new_arcs, LONG number)
#else
void replace_weaker_arc( net, newarc, tail, head, cost, red_cost, max_new_par_residual_new_arcs, number)
     network *net;
     arc_t *newarc;
     node_t *tail;
     node_t *head;
     cost_t cost;
     cost_t red_cost;
     LONG max_new_par_residual_new_arcs;
     LONG number;
#endif
{
    LONG pos;
    LONG cmp;
    newarc[0].tail     = tail;
    newarc[0].head     = head;
    newarc[0].org_cost = cost;
    newarc[0].cost     = cost;
    newarc[0].flow     = (flow_t)red_cost; 
    newarc[0].id       = number;
    pos = 1;
    cmp = (newarc[1].flow > newarc[2].flow) ? 2 : 3;
    while( cmp <= max_new_par_residual_new_arcs && red_cost < newarc[cmp-1].flow )
    {
        newarc[pos-1].tail = newarc[cmp-1].tail;
        newarc[pos-1].head = newarc[cmp-1].head;
        newarc[pos-1].cost = newarc[cmp-1].cost;
        newarc[pos-1].org_cost = newarc[cmp-1].cost;
        newarc[pos-1].flow = newarc[cmp-1].flow;
        newarc[pos-1].id   = newarc[cmp-1].id;
        newarc[cmp-1].tail = tail;
        newarc[cmp-1].head = head;
        newarc[cmp-1].cost = cost;
        newarc[cmp-1].org_cost = cost;
        newarc[cmp-1].flow = (flow_t)red_cost; 
        newarc[cmp-1].id   = number;
        pos = cmp;
        cmp *= 2;
        if( cmp + 1 <= max_new_par_residual_new_arcs )
            if( newarc[cmp-1].flow < newarc[cmp].flow )
                cmp++;
    }
    return;
}   
#if defined AT_HOME
#include <sys/time.h>
double Get_Time( void  ) 
{
    struct timeval tp;
    struct timezone tzp;
    if( gettimeofday( &tp, &tzp ) == 0 )
        return (double)(tp.tv_sec) + (double)(tp.tv_usec)/1.0e6;
    else
        return 0.0;
}
static double wall_time = 0; 
#endif
#ifdef _PROTO_
void calculate_max_redcost(network_t *net, LONG* max_redcost, arc_t*** arcs_pointer_sorted, LONG num_threads)
#else
void calculate_max_redcost(max_redcost, arcs_pointer_sorted, num_threads)
LONG* max_redcost;
arc_t*** arcs_pointer_sorted;
LONG num_threads;
#endif
{
  LONG i;
  *max_redcost = 0;
  for (i = 0; i < num_threads; i++)
  {
    if (arcs_pointer_sorted[i][0]->flow > *max_redcost)
      *max_redcost = arcs_pointer_sorted[i][0]->flow;
  }
}
#ifdef _PROTO_
LONG switch_arcs(network_t* net, LONG *num_del_arcs, arc_t** deleted_arcs, arc_t* arcnew, int thread, LONG max_new_par_residual_new_arcs, LONG size_del, LONG num_threads)
#else
LONG switch_arcs(net, num_del_arcs, deleted_arcs, arcnew, thread, max_new_par_residual_new_arcs, size_del, num_threads)
network_t* net;
LONG *num_del_arcs;
arc_t** deleted_arcs;
arc_t* arcnew;
int thread;
LONG max_new_par_residual_new_arcs;
LONG size_del;
LONG num_threads;
#endif
{
    LONG i, j, h, number_of_arcs, count=0;
    arc_t *test_arc, copy;
   for (i = 0, j = thread; i < num_threads; i++, j = (j + 1) % num_threads)
   {
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
     number_of_arcs = (num_del_arcs[j] < size_del) ? num_del_arcs[j] : size_del;
     for (h = 0; h < number_of_arcs; h++)
     {
       test_arc = &deleted_arcs[j][h];
//       if (test_arc->flow == 0)
//         continue;
       if (!test_arc->ident && ((test_arc->flow < arcnew[0].flow) || (test_arc->flow == arcnew[0].flow &&
           test_arc->id < arcnew[0].id)))
       {
         copy = *test_arc;
         count++;
         *test_arc = arcnew[0];
         replace_weaker_arc( arcnew, copy.tail, copy.head, copy.cost, copy.flow, max_new_par_residual_new_arcs, copy.id );
       }
     }
   }
   return count;
}
#ifdef _PROTO_
LONG price_out_impl( network_t *net )
#else
LONG price_out_impl( net )
     network_t *net;
#endif
{
    LONG i;
    LONG trips;
    LONG new_arcs = 0;
    LONG resized = 0;
    LONG latest;
    LONG min_impl_duration = 15;
    LONG max_new_par_residual_new_arcs;
    int thread;
    LONG *new_arcs_array;
    LONG id, list_size, *num_del_arcs;
    arc_p **arcs_pointer_sorted, *deleted_arcs;
    LONG max_redcost;
    short first_replace = 1, local_first_replace;
    LONG count = 0;
    LONG num_switch_iterations;
  LONG size_del;
    register list_elem *first_list_elem;
    register list_elem *new_list_elem;
    register list_elem* iterator;
    register cost_t bigM = net->bigM;
    register cost_t head_potential;
    register cost_t arc_cost = 30;
    register cost_t red_cost;
    register cost_t bigM_minus_min_impl_duration;
    register arc_t *arcout, *arcin, *arcnew, *stop, *sorted_array, *arc;
    register node_t *tail, *head;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
    LONG num_threads = omp_get_max_threads();
#else
    LONG num_threads = 1;
#endif
    new_arcs_array = (LONG*) malloc(num_threads * sizeof(LONG));
    num_del_arcs = (LONG*) malloc(num_threads * sizeof(LONG));
    arcs_pointer_sorted = (arc_p**) malloc(num_threads * sizeof(arc_p*));
    deleted_arcs = (arc_p*) malloc(num_threads * sizeof(arc_p));
#if defined AT_HOME
    wall_time -= Get_Time();
#endif
    bigM_minus_min_impl_duration = (cost_t)bigM - min_impl_duration;
    if( net->n_trips <= MAX_NB_TRIPS_FOR_SMALL_NET )
    {
      if( net->m + net->max_new_m > net->max_m 
          &&
          (net->n_trips*net->n_trips)/2 + net->m > net->max_m
          )
      {
        resized = 1;
        if( resize_prob( net ) )
          return -1;
        refresh_neighbour_lists( net, &getOriginalArcPosition );
      }
    }
    else
    {
      if( net->m + net->max_new_m > net->max_m 
          &&
          (net->n_trips*net->n_trips)/2 + net->m > net->max_m
          )
      {
        resized = 1;
        if( resize_prob( net ) )
          return -1;
        refresh_neighbour_lists( net, &getOriginalArcPosition );
      }
    }
    if (net->n_trips <= MAX_NB_TRIPS_FOR_SMALL_NET)
        num_switch_iterations = ITERATIONS_FOR_SMALL_NET;
    else
        num_switch_iterations = ITERATIONS_FOR_BIG_NET;
    sorted_array = net->sorted_arcs;
    if (!sorted_array)
      return -1;
    max_new_par_residual_new_arcs = net->max_residual_new_m / num_threads;
  first_replace = 1;
    size_del = net->max_m/num_threads;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel private(local_first_replace,count,arc,max_redcost,list_size,id, thread, stop, red_cost, arcin, head_potential, iterator, head, tail, latest, new_list_elem, first_list_elem, arcout, i, arcnew, trips )
#endif
    {
      //printf("del %d\n", size_del);
      local_first_replace = 1;
      max_redcost = 0;
      count =0;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
  thread = omp_get_thread_num();
#else
  thread = 0;
#endif
      deleted_arcs[thread] = &sorted_array[size_del * thread];
      num_del_arcs[thread] = 0;
        new_arcs_array[thread] = 0;
      arcnew = net->stop_arcs + thread * max_new_par_residual_new_arcs;
      trips = net->n_trips;
      id = 0;
      list_size = -1;
      arcs_pointer_sorted[thread] = (arc_p*) calloc (max_new_par_residual_new_arcs, sizeof(arc_p));
      for (i = 0; i < max_new_par_residual_new_arcs; i++) {
        arcs_pointer_sorted[thread][i] = &arcnew[i];
      }
      arcout = net->arcs;
      for( i = 0; i < trips && arcout[1].ident == FIXED; i++, arcout += 3);
      first_list_elem = (list_elem *)NULL;
      for( ; i < trips; i++, arcout += 3 )
      {
        if (!first_replace) {
            calculate_max_redcost(net, &max_redcost, arcs_pointer_sorted, num_threads);
        }
        if ( i % num_switch_iterations == 0) {
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
            calculate_max_redcost(net, &max_redcost, arcs_pointer_sorted, num_threads);
            if (!first_replace) {
              //printf("thread %d count %ld del_size %ld\n", thread, count, size_del);
              num_del_arcs[thread] = count;
              switch_arcs(net, num_del_arcs, deleted_arcs, arcnew, thread, max_new_par_residual_new_arcs, size_del, num_threads);
              count = 0;
              num_del_arcs[thread] = 0;
          }
        }
        if( arcout[1].ident != FIXED )
        {
          new_list_elem = (list_elem*) calloc(1, sizeof(list_elem));
          new_list_elem->next = first_list_elem;
          new_list_elem->arc = arcout + 1;
          first_list_elem = new_list_elem;
          list_size++;
        }
        if( arcout->ident == FIXED || i % num_threads != thread)
        {
          id += list_size;
          continue;
        }
        head = arcout->head;
        latest = head->time - arcout->org_cost
            + (LONG)bigM_minus_min_impl_duration;
        head_potential = head->potential;
        iterator = first_list_elem->next;
        while( iterator )
        {
          arcin = iterator->arc;
          tail = arcin->tail;
          if( tail->time + arcin->org_cost > latest )
          {
            iterator = iterator->next;
            id++;
            continue;
          }
          red_cost = arc_cost - tail->potential + head->potential;
          if( red_cost < 0 )
          {
            if( new_arcs_array[thread] < max_new_par_residual_new_arcs)
            {
              insert_new_arc( arcnew, new_arcs_array[thread], tail, head,
                  arc_cost, red_cost, net->m, id);
              new_arcs_array[thread]++;
            }
            else if( (cost_t)arcnew[0].flow > red_cost ) {
              if (local_first_replace) {
                first_replace = 0;
                local_first_replace = 0;
              }
              deleted_arcs[thread][num_del_arcs[thread]] = arcnew[0];
              num_del_arcs[thread]++;
              count++;
              replace_weaker_arc( arcnew, tail, head, arc_cost, red_cost, max_new_par_residual_new_arcs, id);
            }
            else if (red_cost < max_redcost ) {
              arc = &deleted_arcs[thread][num_del_arcs[thread]++];
              arc->tail     = tail;
              arc->head     = head;
              arc->org_cost = arc_cost;
              arc->cost     = arc_cost;
              arc->flow     = (flow_t)red_cost;
              arc->id       = id;
              count++;
            }
            if (num_del_arcs[thread] == size_del)
            {
              num_del_arcs[thread] = 0;
            }
          }
          iterator = iterator->next;
          id++;
        }
      }
      num_del_arcs[thread] = count;
      while (!first_replace) {
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
        first_replace = 1;
        //printf("Schleife vorher thread %d count %ld del_size %ld\n", thread, count, size_del);
          count = switch_arcs(net, num_del_arcs, deleted_arcs, arcnew, thread, max_new_par_residual_new_arcs, size_del, num_threads);
          //printf("Schleife nachher thread %d count %ld del_size %ld\n", thread, count, size_del);
          if (count)
            first_replace = 0;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
      }
      while (first_list_elem->next) {
        new_list_elem = first_list_elem;
        first_list_elem = first_list_elem->next;
        free(new_list_elem);
      }
      free(first_list_elem);
#if defined(SPEC)
        spec_qsort(arcs_pointer_sorted[thread], new_arcs_array[thread], sizeof(arc_p),
                (int (*)(const void *, const void *))arc_compare);
#else
        qsort(arcs_pointer_sorted[thread], new_arcs_array[thread], sizeof(arc_p),
                (int (*)(const void *, const void *))arc_compare);
#endif
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#pragma omp master
#endif
      {
          marc_arcs(net, &new_arcs, new_arcs_array, arcs_pointer_sorted);
      }
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
  free(arcs_pointer_sorted[thread]);
      if( new_arcs_array[thread] )
      {
        arcnew = net->stop_arcs + thread * max_new_par_residual_new_arcs;
        stop = arcnew + new_arcs_array[thread];
        if( resized )
        {
          for( ; arcnew != stop; arcnew++ )
          {
            if (arcnew->flow == 1) {
                arcnew->flow = (flow_t)0;
                arcnew->ident = AT_LOWER;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp critical
#endif
	        sorted_array[getArcPosition(net, arcnew->id)] = *arcnew;
            }
          }
        }
        else
        {
          for( ; arcnew != stop; arcnew++ )
          {
            if (arcnew->flow == 1) {
              arcnew->flow = (flow_t)0;
              arcnew->ident = AT_LOWER;
              arcnew->nextout = arcnew->tail->firstout;
              arcnew->tail->firstout = arcnew;
              arcnew->nextin = arcnew->head->firstin;
              arcnew->head->firstin = arcnew;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp critical
#endif
              sorted_array[getArcPosition(net, arcnew->id)] = *arcnew;
            }
          }
        }
      }
    }
        net->m_impl += new_arcs;
        net->max_residual_new_m -= new_arcs;
        refreshPositions(net, &getArcPosition, net->m);
        net->m = net->m + new_arcs;
        net->stop_arcs = net->arcs + net->m;
#ifdef DEBUG
       arc_t* arc = net->arcs;
       for (i=0;arc < net->stop_arcs; arc++, i++)
         if (!arc->head) {
           printf("arc %ld is null\n", i);
         }
#endif
#if defined AT_HOME
    wall_time += Get_Time();
    printf( "total time price_out_impl(): %0.0f\n", wall_time );
#endif
    free(new_arcs_array);
    free(num_del_arcs);
    free(arcs_pointer_sorted);
    free(deleted_arcs);
    return new_arcs;
}   
#ifdef _PROTO_
LONG suspend_impl( network_t *net, cost_t threshold, LONG all )
#else
LONG suspend_impl( net, threshold, all )
     network_t *net;
     cost_t threshold;
     LONG all;
#endif
{
    LONG susp;
    cost_t red_cost;
    arc_t *arc;
    LONG stop, startid;
    net->max_elems = K;
    net->nr_group = ( (net->m -1) / K ) + 1;
    net->full_groups = net->nr_group - (K - (net->m % K));
  while (net->full_groups < 0) {
    net->full_groups = net->nr_group + net->full_groups;
    net->max_elems--;
  }
    if( all ) {
        susp = net->m_impl;
    }
    else
    {
        startid = net->m - net->m_impl;
        for( stop = net->m - net->m_impl, susp = 0; stop < net->m;  stop++)
        {
          arc = net->arcs + getArcPosition(net, stop);
            if( arc->ident == AT_LOWER )
                red_cost = arc->cost - arc->tail->potential
                        + arc->head->potential;
            else
            {
                red_cost = (cost_t)-2;
                if( arc->ident == BASIC )
                {
                    if( !(arc->tail->basic_arc == arc) )
                      arc->head->basic_arc = arc;
                }
            }
            if( red_cost > threshold ) {
                susp++;
                arc->id = DELETED;
            }
            else
            {
              arc->id = startid;
                startid++;
            }
        }
    }
#if defined AT_HOME
    printf( "\nremove %ld arcs\n\n", susp );
    fflush( stdout );
#endif
    if( susp )
    {
        net->m_impl -= susp;
        net->max_residual_new_m += susp;
        net->max_elems = K;
        net->nr_group = ( (net->m - susp -1) / K ) + 1;
        if ((net->m - susp) % K != 0)
           net->full_groups = net->nr_group - (K - ((net->m - susp) % K));
        else
           net->full_groups = net->nr_group;
      while (net->full_groups < 0) {
        net->full_groups = net->nr_group + net->full_groups;
        net->max_elems--;
      }
        refreshPositions(net, &getOriginalArcPosition, net->m);
      net->m -= susp;
        net->stop_arcs -= susp;
        refresh_neighbour_lists( net, &getOriginalArcPosition );
    }
    return susp;
}
//#include "mcf/mcfutil.c"
/**************************************************************************
MCFUTIL.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:46:54 2010 by Loebel (opt0.zib.de)  */
/*  $Id: mcfutil.c,v 1.12 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/mcfutil.h"
#ifdef _PROTO_
void refresh_neighbour_lists( network_t *net, LONG (*getPos)(network_t*, LONG) )
#else
void refresh_neighbour_lists( net )
    network_t *net;
#endif
{
    node_t *node;
    arc_t *arc;
    void *stop;
    int i;
    node = net->nodes;
    for( stop = (void *)net->stop_nodes; node < (node_t *)stop; node++ )
    {
        node->firstin = (arc_t *)NULL;
        node->firstout = (arc_t *)NULL;
    }
    arc = net->arcs;
    for( i = 0; i < net->m; i++, arc = &net->arcs[getPos(net, i)] )
    {
        arc->nextout = arc->tail->firstout;
        arc->tail->firstout = arc;
        arc->nextin = arc->head->firstin;
        arc->head->firstin = arc;
    }
    return;
}
#ifdef _PROTO_
double flow_cost( network_t *net )
#else
double flow_cost( net )
    network_t *net;
#endif
{
    arc_t *arc;
    node_t *node;
    LONG fleet = 0;
    int i;
    cost_t operational_cost = 0;
    arc = net->arcs;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for
#endif
    for( i = 0; i < net->m ; i++ )
    {
        if( arc[i].ident == AT_UPPER )
            arc[i].flow = (flow_t)1;
        else
            arc[i].flow = (flow_t)0;
    }
    node = net->nodes;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for
#endif
    for( i = 1; i <= net->n; i++) {
        node[i].basic_arc->flow = node[i].flow;
    }
    arc = net->arcs;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for reduction(+ : fleet, operational_cost)
#endif
    for( i = 0; i < net->m; i++ )
    {
        if( arc[i].flow )
        {
            if( !(arc[i].tail->number < 0 && arc[i].head->number > 0) )
            {
                if( !arc[i].tail->number )
                {
                    operational_cost += (arc[i].cost - net->bigM);
                    fleet++;
                }
                else
                    operational_cost += arc[i].cost;
            }
        }
    }
    return (double)fleet * (double)net->bigM + (double)operational_cost;
}
static LONG old_group = 0;
static LONG old_Arc = 0;
#ifdef _PROTO_
LONG start()
#else
start()
#endif
{
    old_group = 0;
    old_Arc = 0;
    return 0;
}
#ifdef _PROTO_
LONG getArcPosition(network_t *net, LONG actArc)
#else
int getArcPosition(net, actArc)
network_t *net;
LONG actArc;
#endif
{
  LONG result, akt_group;
  akt_group = actArc % net->nr_group;
  if (akt_group > net->full_groups) {
    result = (actArc / net->nr_group) + (net->full_groups * net->max_elems + (akt_group - net->full_groups) * (net->max_elems -1));
  }
  else {
    result = (actArc / net->nr_group) + (akt_group * net->max_elems);
  }
    return result;
}
#ifdef _PROTO_
LONG getOriginalArcPosition(network_t *net, LONG actArc)
#else
getOriginalArcPosition(net, actArc)
network_t *net;
LONG actArc;
#endif
{
  return actArc;
}
#ifdef _PROTO_
double flow_org_cost( network_t *net )
#else
double flow_org_cost( net )
    network_t *net;
#endif
{
    arc_t *arc;
    node_t *node;
    int i;
    LONG fleet = 0;
    cost_t operational_cost = 0;
    arc = net->arcs;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for
#endif
    for( i = 0; i < net->m; i++ )
    {
        if( arc[i].ident == AT_UPPER )
            arc[i].flow = (flow_t)1;
        else
            arc[i].flow = (flow_t)0;
    }
    node = net->nodes;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for reduction(+: fleet, operational_cost)
#endif
    for( i = 0; i < net->n; i++ )
        node[i].basic_arc->flow = node[i].flow;
    arc = net->arcs;
    for( i = 0; i < net->m; i++ )
    {
        if( arc[i].flow )
        {
            if( !(arc[i].tail->number < 0 && arc[i].head->number > 0) )
            {
                if( !arc[i].tail->number )
                {
                    operational_cost += (arc[i].org_cost - net->bigM);
                    fleet++;
                }
                else
                    operational_cost += arc[i].org_cost;
            }
        }
    }
printf("ORG_COST: %f", (double)fleet * (double)net->bigM + (double)operational_cost);
    return (double)fleet * (double)net->bigM + (double)operational_cost;
}
#ifdef _PROTO_
LONG primal_feasible( network_t *net )
#else
LONG primal_feasible( net )
    network_t *net;
#endif
{
    void *stop;
    node_t *node;
    arc_t *dummy = net->dummy_arcs;
    arc_t *stop_dummy = net->stop_dummy;
    arc_t *arc;
    flow_t flow;
    node = net->nodes;
    stop = (void *)net->stop_nodes;
    for( node++; node < (node_t *)stop; node++ )
    {
        arc = node->basic_arc;
        flow = node->flow;
        if( arc >= dummy && arc < stop_dummy )
        {
            if( ABS(flow) > (flow_t)net->feas_tol )
            {
                printf( "PRIMAL NETWORK SIMPLEX: " );
                printf( "artificial arc with nonzero flow, node %d (%" PRId64 ")\n",
                        node->number, flow );
            }
        }
        else
        {
            if( flow < (flow_t)(-net->feas_tol)
               || flow - (flow_t)1 > (flow_t)net->feas_tol )
            {
                printf( "PRIMAL NETWORK SIMPLEX: " );
                printf( "basis primal infeasible (%" PRId64 ")\n", flow );
                net->feasible = 0;
                return 1;
            }
        }
    }
    net->feasible = 1;
    return 0;
}
#ifdef _PROTO_
LONG dual_feasible( network_t *net )
#else
LONG dual_feasible(  net )
    network_t *net;
#endif
{
    arc_t         *arc;
    arc_t         *stop     = net->stop_arcs;
    cost_t        red_cost;
    LONG i = 0;
    for( i= 0, arc = net->arcs; arc < stop; arc++, i++ )
    {
        red_cost = arc->cost - arc->tail->potential 
            + arc->head->potential;
        switch( arc->ident )
        {
        case BASIC:
#ifdef AT_ZERO
        case AT_ZERO:
            if( ABS(red_cost) > (cost_t)net->feas_tol )
#ifdef DEBUG
                printf("%d %d %d %ld\n", arc->tail->number, arc->head->number,
                       arc->ident, red_cost );
#else
                goto DUAL_INFEAS;
#endif
            break;
#endif
        case AT_LOWER:
            if( red_cost < (cost_t)-net->feas_tol )
#ifdef DEBUG
                printf("%d %d %d %ld\n", arc->tail->number, arc->head->number,
                       arc->ident, red_cost );
#else
                goto DUAL_INFEAS;
#endif
            break;
        case AT_UPPER:
            if( red_cost > (cost_t)net->feas_tol )
#ifdef DEBUG
                printf("%d %d %d %ld\n", arc->tail->number, arc->head->number,
                       arc->ident, red_cost );
#else
                goto DUAL_INFEAS;
#endif
            break;
        case FIXED:
        default:
            break;
        }
    }
    return 0;
DUAL_INFEAS:
    fprintf( stderr, "DUAL NETWORK SIMPLEX: " );
    fprintf( stderr, "basis dual infeasible\n" );
    return 1;
}
#ifdef _PROTO_
LONG getfree( 
            network_t *net
            )
#else
LONG getfree( net )
     network_t *net;
#endif
{  
    FREE( net->nodes );
    FREE( net->arcs );
    FREE( net->dummy_arcs );
    FREE( net->sorted_arcs);
    net->nodes = net->stop_nodes = NULL;
    net->arcs = net->stop_arcs = NULL;
    net->dummy_arcs = net->stop_dummy = NULL;
    net->sorted_arcs = NULL;
    return 0;
}
//#include "mcf/output.c"
/**************************************************************************
OUTPUT.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Wed May 26 00:05:32 2010 by Loebel (opt0.zib.de)  */
/*  $Id: output.c,v 1.12 2010/05/26 08:26:29 bzfloebe Exp $  */
#include "mcf/output.h"
#ifdef _PROTO_
LONG write_circulations(
                   char *outfile,
                   network_t *net
                   )
#else
LONG write_circulations( outfile, net )
     char *outfile;
     network_t *net;
#endif 
{
  FILE *out = NULL;
  arc_t *block;
  arc_t *arc;
  arc_t *arc2;
  arc_t *first_impl = net->stop_arcs - net->m_impl;
  if(( out = fopen( outfile, "w" )) == NULL )
    return -1;
  refresh_neighbour_lists( net, &getArcPosition );
  for( block = net->nodes[net->n].firstout; block; block = block->nextout )
  {
    if( block->flow )
    {
      fprintf( out, "()\n" );
      arc = block;
      while( arc )
      {
        if( arc >= first_impl )
          fprintf( out, "***\n" );
        fprintf( out, "%d\n", - arc->head->number );
        arc2 = arc->head[net->n_trips].firstout; 
        for( ; arc2; arc2 = arc2->nextout )
          if( arc2->flow )
            break;
        if( !arc2 )
        {
          fclose( out );
          return -1;
        }
        if( arc2->head->number )
          arc = arc2;
        else
          arc = NULL;
      }
    }
  }
  fclose(out);
  return 0;
}
#ifdef _PROTO_
LONG write_objective_value(
                   char *outfile,
                   network_t *net
                   )
#else
LONG write_objective_value( outfile, net )
     char *outfile;
     network_t *net;
#endif 
{
  FILE *out = NULL;
  if(( out = fopen( outfile, "w" )) == NULL )
    return -1;
  fprintf( out, "%.0f\n", flow_cost(net) );
  fclose(out);
  return 0;
}
//#include "mcf/pbeampp.c"
/**************************************************************************
PBEAMPP.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:47:14 2010 by Loebel (opt0.zib.de)  */
/*  $Id: pbeampp.c,v 1.11 2010/05/25 21:58:44 bzfloebe Exp $  */
#if defined(SPEC)
# include "mcf/spec_qsort.h"
#endif
#include "mcf/pbeampp.h"
#include "mcf/mcfutil.h"
static arc_t* full_group_end_arc;
#ifdef _PROTO_
void set_static_vars(network_t *net, arc_t* arcs)
#else
void set_static_vars(net, arcs)
network_t *net;
arc_t* arcs;
#endif
{
  full_group_end_arc = arcs + net->full_groups * net->max_elems;
}
#ifdef _PROTO_
int bea_is_dual_infeasible( arc_t *arc, cost_t red_cost )
#else
int bea_is_dual_infeasible( arc, red_cost )
    arc_t *arc;
    cost_t red_cost;
#endif
{
    return(    (red_cost < 0 && arc->ident == AT_LOWER)
            || (red_cost > 0 && arc->ident == AT_UPPER) );
}
#ifdef _PROTO_
int cost_compare( BASKET **b1, BASKET **b2 )
#else
int cost_compare( b1, b2 )
    BASKET **b1;
    BASKER **b2;
#endif
{
  if( (*b1)->abs_cost < (*b2)->abs_cost )
    return 1;
  if( (*b1)->abs_cost > (*b2)->abs_cost )
    return -1;
  if( (*b1)->a->id > (*b2)->a->id )
    return 1;
  else
    return -1;
}
#ifdef _PROTO_
BASKET *primal_bea_mpp( LONG m,  arc_t *arcs, arc_t *stop_arcs,
                          LONG* basket_sizes, BASKET** perm, int thread, arc_t** end_arc, LONG step, LONG num_threads, LONG max_elems)
#else
arc_t *primal_bea_mpp( m, arcs, stop_arcs, basket_sizes, perm, thread, end_arc, step, num_threads, max_elems )
LONG m;
arc_t *arcs;
arc_t *stop_arcs;
LONG *basket_sizes;
BASKET** perm;
int thread;
arc_t** end_arc;
LONG step;
LONG num_threads;
LONG max_elems;
#endif
{
    LONG i, j, count,  global_basket_size, next;
    arc_t *arc, *old_end_arc;
    cost_t red_cost;
       for( i = 1, next = 0; i <= B && i <= basket_sizes[thread]; i++ )
       {
           arc = perm[i]->a;
           count = perm[i]->number;
           red_cost = arc->cost - arc->tail->potential + arc->head->potential;
           if( count > 0 && ((red_cost < 0 && arc->ident == AT_LOWER)
               || (red_cost > 0 && arc->ident == AT_UPPER)) )
           {
               next++;
               perm[next]->a = arc;
               perm[next]->cost = red_cost;
               perm[next]->abs_cost = ABS(red_cost);
               perm[next]->number = 0;
           }
        }
        basket_sizes[thread] = next;
        old_end_arc = *end_arc;
    NEXT:
    arc = *end_arc + step;
    if (*end_arc >= full_group_end_arc)
      *end_arc = *end_arc + max_elems - 1;
    else
      *end_arc = *end_arc + max_elems;
       for ( ; arc < *end_arc; arc += num_threads) {
      if( arc->ident > BASIC)
      {
        /* red_cost = bea_compute_red_cost( arc ); */
        red_cost = arc->cost - arc->tail->potential + arc->head->potential;
        if( bea_is_dual_infeasible( arc, red_cost ) )
        {
          basket_sizes[thread]++;
          perm[basket_sizes[thread]]->a = arc;
          perm[basket_sizes[thread]]->cost = red_cost;
          perm[basket_sizes[thread]]->abs_cost = ABS(red_cost);
          perm[basket_sizes[thread]]->number = 0;
        }
      }
       }
       if( *end_arc >= stop_arcs ) {
           *end_arc = arcs;
       }
    if (*end_arc != old_end_arc) {
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
      global_basket_size = 0;
      for (j = 0; j < num_threads; j++) {
        global_basket_size+=basket_sizes[j];
      }
      if ( global_basket_size >= B) {
        goto READY;
      }
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp barrier
#endif
        goto NEXT;
    }
   READY:
   perm[basket_sizes[thread] + 1]->number = -1;
    if (basket_sizes[thread] == 0) {
      return NULL;
    }
#if defined(SPEC)
    spec_qsort(perm + 1, basket_sizes[thread], sizeof(BASKET*),
            (int (*)(const void *, const void *))cost_compare);
#else
    qsort(perm + 1, basket_sizes[thread], sizeof(BASKET*),
            (int (*)(const void *, const void *))cost_compare);
#endif
    return perm[1];
}
//#include "mcf/pbla.c"
/**************************************************************************
PBLA.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:47:25 2010 by Loebel (opt0.zib.de)  */
/*  $Id: pbla.c,v 1.11 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/pbla.h"
#define TEST_MIN( nod, ex, comp, op ) \
{ \
      if( *delta op (comp) ) \
      { \
            iminus = nod; \
            *delta = (comp); \
            *xchange = ex; \
      } \
}
#ifdef _PROTO_
node_t *primal_iminus( 
                      flow_t *delta,
                      LONG *xchange,
                      node_t *iplus, 
                      node_t*jplus,
                      node_t **w
                    )
#else
node_t *primal_iminus( delta, xchange, iplus, jplus, w )
    flow_t *delta;
    LONG *xchange;
    node_t *iplus, *jplus;
    node_t **w;
#endif
{
    node_t *iminus = NULL;
    while( iplus != jplus )
    {
        if( iplus->depth < jplus->depth )
        {
            if( iplus->orientation )
                TEST_MIN( iplus, 0, iplus->flow, > )
            else if( iplus->pred->pred )
                TEST_MIN( iplus, 0, (flow_t)1 - iplus->flow, > )
            iplus = iplus->pred;
        }
        else
        {
            if( !jplus->orientation )
                TEST_MIN( jplus, 1, jplus->flow, >= )
            else if( jplus->pred->pred )
                TEST_MIN( jplus, 1, (flow_t)1 - jplus->flow, >= )
            jplus = jplus->pred;
        }
    } 
    *w = iplus;
    return iminus;
}
//#include "mcf/pflowup.c"
/**************************************************************************
PFLOWUP.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:47:38 2010 by Loebel (opt0.zib.de)  */
/*  $Id: pflowup.c,v 1.11 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/pflowup.h"
#ifdef _PROTO_
void primal_update_flow( 
                 node_t *iplus,
                 node_t *jplus,
                 node_t *w
                 )
#else
void primal_update_flow( iplus, jplus, w )
    node_t *iplus, *jplus;
    node_t *w; 
#endif
{
    for( ; iplus != w; iplus = iplus->pred )
    {
        if( iplus->orientation )
            iplus->flow = (flow_t)0;
        else
            iplus->flow = (flow_t)1;
    }
    for( ; jplus != w; jplus = jplus->pred )
    {
        if( jplus->orientation )
            jplus->flow = (flow_t)1;
        else
            jplus->flow = (flow_t)0;
    }
}
//#include "mcf/psimplex.c"
/**************************************************************************
PSIMPLEX.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:47:54 2010 by Loebel (opt0.zib.de)  */
/*  $Id: psimplex.c,v 1.10 2010/05/25 21:58:44 bzfloebe Exp $  */
#undef DEBUG
#include "mcf/psimplex.h"
static double runtime = 0;
static BASKET    **opt_basket;
static BASKET    ***perm_p;
static LONG      *basket_sizes;
static LONG      opt = 0;
static BASKET    *basket;
#ifdef _PROTO_
void markBaskets(LONG num_threads)
#else
void markBaskets(num_threads)
LONG num_threads;
#endif
{
    LONG  i, j, max_pos = 0;
    BASKET* max, *act;
    for ( i=1; i<=B; i++) {
      if ((*perm_p[0])->number >= 0) {
          max = (*perm_p[0]);
          max_pos = 0;
      }
      else {
        max = 0;
      }
        for (j = 1; j < num_threads; j++) {
          act = *perm_p[j];
          if (act->number >= 0) {
                if (!max || cost_compare(&act, &max) < 0) {
                    max = act;
                    max_pos = j;
                }
          }
        }
      if (!max) {
        return;
      }
        max->number = i;
        (perm_p[max_pos])++;
    }
}
#if defined AT_HOME
#include <sys/time.h>
double Get_Time2( void  )
{
    struct timeval tp;
    struct timezone tzp;
    if( gettimeofday( &tp, &tzp ) == 0 )
        return (double)(tp.tv_sec) + (double)(tp.tv_usec)/1.0e6;
    else
        return 0.0;
}
#endif
#ifdef _PROTO_
void worker(network_t *net, int thread, int num_threads)
#else
void worker(net, thread)
network_t *net;
int thread;
#endif
{
  arc_t         *arcs          = net->arcs;
  arc_t         *stop_arcs     = net->stop_arcs;
  LONG          m = net->m;
  LONG          *iterations = &(net->iterations);
  BASKET        *perm[K + B +1];
  arc_t         *end_arc = net->arcs;
  LONG          i, j;
  basket_sizes[thread] = 0;
  for( j = thread * (K/num_threads+B+1 + PUFFER) + 1, i=1; i < K/num_threads+B+1; i++, j++)
    perm[i] = &(basket[j]);
  while (!opt) {
    opt_basket[thread] = primal_bea_mpp( m, arcs, stop_arcs, basket_sizes, perm, thread, &end_arc, (*iterations + thread) % num_threads, num_threads, net->max_elems);
    perm_p[thread] = perm + 1;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
    #pragma omp barrier
#endif
    if (thread == 1)
      markBaskets(num_threads);
    // master must do some work here
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
    #pragma omp barrier
#endif
  }
}
#ifdef _PROTO_
void master(network_t *net, int num_threads)
#else
void master(net)
network_t *net;
#endif
{
  flow_t        delta;
  flow_t        new_flow;
  LONG          xchange;
  LONG          new_orientation;
  node_t        *iplus;
  node_t        *jplus;
  node_t        *iminus;
  node_t        *jminus;
  node_t        *w;
  arc_t         *bea;
  arc_t         *bla;
  arc_t         *arcs          = net->arcs;
  arc_t         *stop_arcs     = net->stop_arcs;
  node_t        *temp;
  LONG          m = net->m;
  LONG          new_set;
  cost_t        red_cost_of_bea;
  LONG          *iterations = &(net->iterations);
  LONG          *bound_exchanges = &(net->bound_exchanges);
  BASKET*       max_basket;
  BASKET        *perm[K + B +1];
  arc_t         *end_arc = net->arcs;
  LONG         i, j;
#if defined AT_HOME
  double time1 = 0, start;
    double startTime, endTime;
    startTime = Get_Time2();
#endif
  basket_sizes[0] = 0;
  for( j = 1, i=1; i < K/num_threads+B+1; i++, j ++ )
    perm[i] = &(basket[j]);
#if defined AT_HOME
    start = Get_Time2();
#endif
  while( !opt )
  {
#if defined AT_HOME
    time1 += Get_Time2() - start;
#endif
    opt_basket[0] = primal_bea_mpp( m, arcs, stop_arcs, basket_sizes, perm, 0, &end_arc, (*iterations) % num_threads, num_threads, net->max_elems);
#if defined AT_HOME
    start = Get_Time2();
#endif
    perm_p[0] =  perm + 1;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
    #pragma omp barrier
#endif
    max_basket = 0;
    for (i = 0; i< num_threads; i++) {
      if ((!max_basket && opt_basket[i]) || (opt_basket[i] && cost_compare(&opt_basket[i], &max_basket) < 0)) {
        max_basket = opt_basket[i];
      }
    }
    if( !max_basket )
    {
      red_cost_of_bea = 0;
    }
    else {
      red_cost_of_bea = max_basket->cost;
      bea = max_basket->a;
      if (num_threads == 1)
          markBaskets(num_threads);
    }
    if( red_cost_of_bea != 0)
    {
      (*iterations)++;
      //printf("it %d\n", *iterations);
#ifdef DEBUG
      printf( "it %ld: bea = (%ld,%ld), red_cost = %ld\n",
          *iterations, bea->tail->number, bea->head->number,
          red_cost_of_bea );
#endif
      if( red_cost_of_bea > ZERO )
      {
        iplus = bea->head;
        jplus = bea->tail;
      }
      else
      {
        iplus = bea->tail;
        jplus = bea->head;
      }
      delta = (flow_t)1;
      iminus = primal_iminus( &delta, &xchange, iplus,
          jplus, &w );
      if( !iminus )
      {
        (*bound_exchanges)++;
        if( bea->ident == AT_UPPER)
          bea->ident = AT_LOWER;
        else
          bea->ident = AT_UPPER;
        if( delta )
          primal_update_flow( iplus, jplus, w );
      }
      else
      {
        if( xchange )
        {
          temp = jplus;
          jplus = iplus;
          iplus = temp;
        }
        jminus = iminus->pred;
        bla = iminus->basic_arc;
        if( xchange != iminus->orientation )
          new_set = AT_LOWER;
        else
          new_set = AT_UPPER;
        if( red_cost_of_bea > 0 )
          new_flow = (flow_t)1 - delta;
        else
          new_flow = delta;
        if( bea->tail == iplus )
          new_orientation = UP;
        else
          new_orientation = DOWN;
        update_tree( !xchange, new_orientation,
            delta, new_flow, iplus, jplus, iminus,
            jminus, w, bea, red_cost_of_bea,
            (flow_t)net->feas_tol );
        bea->ident = BASIC;
        bla->ident = new_set;
      }
    }
    else
      opt = 1;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
    #pragma omp barrier
#endif
  }
#if defined AT_HOME
    endTime = Get_Time2();
    runtime += endTime - startTime;
    printf("runtime simplex            : %.2f sec\n", endTime - startTime);
    printf("runtime master thread      : %.2f sec\n", time1);
    printf("runtime global simplex     : %.2f sec\n", runtime);
#endif
}
#ifdef _PROTO_
LONG primal_net_simplex( network_t *net )
#else
LONG primal_net_simplex(  net )
    network_t *net;
#endif
{
   int thread;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
   int num_threads = omp_get_max_threads();
#else
  int num_threads = 1;
#endif
    perm_p = (BASKET***)   calloc(num_threads, sizeof(BASKET**));
    opt_basket = (BASKET**) calloc(num_threads, sizeof(BASKET*));
    basket_sizes = (LONG*) calloc(num_threads, sizeof(LONG));
    basket = (BASKET*) calloc(num_threads * (K/num_threads + B + PUFFER + 1), sizeof(BASKET));
  set_static_vars(net, net->arcs);
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel shared(net, num_threads)  private(thread)  default(none)
#endif
  {
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
  thread = omp_get_thread_num();
#else
  thread = 0;
#endif  
      if (thread == 0)
        master(net, num_threads);
      else
        worker(net, thread, num_threads);
  }
    primal_feasible( net );
    dual_feasible( net );
    opt = 0;
    free(perm_p);
    free(opt_basket);
    free(basket_sizes);
    free(basket);
    return 0;
}
//#include "mcf/pstart.c"
/**************************************************************************
PSTART.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:48:03 2010 by Loebel (opt0.zib.de)  */
/*  $Id: pstart.c,v 1.11 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/pstart.h"
#ifdef _PROTO_ 
LONG primal_start_artificial( network_t *net )
#else
LONG primal_start_artificial( net )
    network_t *net;
#endif
{      
    node_t *node, *root;
    arc_t *arc;
    int i;
    root = node = net->nodes; node++;
    root->basic_arc = NULL;
    root->pred = NULL;
    root->child = node;
    root->sibling = NULL;
    root->sibling_prev = NULL;
    root->depth = (net->n) + 1;
    root->orientation = 0;
    root->potential = (cost_t) -MAX_ART_COST;
    root->flow = ZERO;
    arc = net->arcs;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for
#endif
    for( i = 0; i < net->m; i++ )
        if( arc[i].ident != FIXED )
            arc[i].ident = AT_LOWER;
    arc = net->dummy_arcs;
#if (defined(_OPENMP) || defined(SPEC_OPENMP)) && !defined(SPEC_SUPPRESS_OPENMP) && !defined(SPEC_AUTO_SUPPRESS_OPENMP)
#pragma omp parallel for
#endif
    for ( i = 0; i < net->n ; i++)
    {
        node[i].basic_arc = &arc[i];
        node[i].pred = root;
        node[i].child = NULL;
        node[i].sibling = &node[i + 1];
        node[i].sibling_prev = &node[i - 1];
        node[i].depth = 1;
        arc[i].cost = (cost_t) MAX_ART_COST;
        arc[i].ident = BASIC;
        node[i].orientation = UP;
        node[i].potential = ZERO;
        arc[i].tail = &node[i];
        arc[i].head = root;
        arc[i].id = DUMMY_ARC;
        node[i].flow = (flow_t)0;
    }
    node--;
    root++;
    node[net->n].sibling = NULL;
    root->sibling_prev = NULL;
    return 0;
}
//#include "mcf/readmin.c"
/**************************************************************************
READMIN.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:48:16 2010 by Loebel (opt0.zib.de)  */
/*  $Id: readmin.c,v 1.17 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/readmin.h"
#ifdef _PROTO_
LONG read_min( network_t *net )
#else
LONG read_min( net )
     network_t *net;
#endif
{                                       
    FILE *in = NULL;
    char instring[201];
    LONG t, h, c;
    LONG i ,actArc = 0;
    arc_t *arc;
    node_t *node;
    if(( in = fopen( net->inputfile, "r")) == NULL )
        return -1;
    fgets( instring, 200, in );
#ifdef SPEC
    if( sscanf( instring, "%" PRId64 " %" PRId64 , &t, &h ) != 2 )
#else
    if( sscanf( instring, "%ld %ld", &t, &h ) != 2 )
#endif
        return -1;
    net->n_trips = t;
    net->m_org = h;
    net->n = (t+t+1); 
    net->m = (t+t+t+h);
    net->max_elems = K;
    net->nr_group = ( (net->m -1) / K ) + 1;
    if (net->m % K != 0)
        net->full_groups = net->nr_group - (K - (net->m % K));
    else
    	net->full_groups = net->nr_group;
  while (net->full_groups < 0) {
    net->full_groups = net->nr_group + net->full_groups;
    net->max_elems--;
  }
    if( net->n_trips <= MAX_NB_TRIPS_FOR_SMALL_NET )
    {
      net->max_m = net->m;
      net->max_new_m = MAX_NEW_ARCS_SMALL_NET;
      net->max_residual_new_m = net->max_m - net->m;
    }
    else
    {
      net->max_m = MAX( net->m + MAX_NEW_ARCS, STRECHT(STRECHT(net->m)) );
      net->max_new_m = MAX_NEW_ARCS_LARGE_NET;
    }
    assert( net->max_new_m >= 3 );
    net->nodes      = (node_t *) calloc( net->n + 1, sizeof(node_t) );
    net->dummy_arcs = (arc_t *)  calloc( net->n,   sizeof(arc_t) );
    net->sorted_arcs  = (arc_t *)  calloc( net->max_m,   sizeof(arc_t) );
    net->arcs       = (arc_t *)  calloc( net->max_m,   sizeof(arc_t) );
    if( !( net->nodes && net->arcs && net->dummy_arcs && net->sorted_arcs) )
    {
      printf( "read_min(): not enough memory\n" );
      getfree( net );
      return -1;
    }
#if defined AT_HOME
    printf( "malloc for nodes         MB %4ld\n",
            (LONG)((net->n + 1)*sizeof(node_t) / 0x100000) );
    printf( "malloc for dummy arcs    MB %4ld\n",
            (LONG)((net->n)*sizeof(arc_t) / 0x100000) );
    printf( "malloc for arcs          MB %4ld\n",
            (LONG)((net->max_m)*sizeof(arc_t) / 0x100000) );
    printf( "malloc for sorting array MB %4ld\n",
            (LONG)((net->max_m)*sizeof(arc_t) / 0x100000) );
    printf( "--------------------------------\n" );
    printf( "heap about               MB %4ld\n\n",
            (LONG)((net->n +1)*sizeof(node_t) / 0x100000)
            +(LONG)((net->n)*sizeof(arc_t) / 0x100000)
            + 2 * (LONG)((net->max_m)*sizeof(arc_t) / 0x100000)
            );
#endif
    net->stop_nodes = net->nodes + net->n + 1; 
    net->stop_arcs  = net->arcs + net->m;
    net->stop_dummy = net->dummy_arcs + net->n;
    node = net->nodes;
    arc = net->arcs;
    for( i = 1; i <= net->n_trips; i++ )
    {
        fgets( instring, 200, in );
#ifdef SPEC
        if( sscanf( instring, "%" PRId64 " %" PRId64 , &t, &h ) != 2 || t > h )
#else
        if( sscanf( instring, "%ld %ld", &t, &h ) != 2 || t > h )
#endif
            return -1;
        node[i].number = -i;
        node[i].flow = (flow_t)-1;
        node[i+net->n_trips].number = i;
        node[i+net->n_trips].flow = (flow_t)1;
        node[i].time = t;
        node[i+net->n_trips].time = h;
        arc->id = actArc;
        arc->tail = &(node[net->n]);
        arc->head = &(node[i]);
        arc->org_cost = arc->cost = (cost_t)(net->bigM+15);
        arc->nextout = arc->tail->firstout;
        arc->tail->firstout = arc;
        arc->nextin = arc->head->firstin;
        arc->head->firstin = arc;
        arc = net->arcs + getArcPosition(net, ++actArc);
        arc->id = actArc;
        arc->tail = &(node[i+net->n_trips]);
        arc->head = &(node[net->n]);
        arc->org_cost = arc->cost = (cost_t)15;
        arc->nextout = arc->tail->firstout;
        arc->tail->firstout = arc;
        arc->nextin = arc->head->firstin;
        arc->head->firstin = arc; 
        arc = net->arcs + getArcPosition(net, ++actArc);
        arc->id = actArc;
        arc->tail = &(node[i]);
        arc->head = &(node[i+net->n_trips]);
        arc->org_cost = arc->cost = (cost_t)(2*MAX(net->bigM,(LONG)BIGM));
        arc->nextout = arc->tail->firstout;
        arc->tail->firstout = arc;
        arc->nextin = arc->head->firstin;
        arc->head->firstin = arc;
        arc = net->arcs + getArcPosition(net, ++actArc);
    }
    if( i != net->n_trips + 1 )
        return -1;
    for( i = 0; i < net->m_org; i++, arc = net->arcs + getArcPosition(net, ++actArc))
    {
        fgets( instring, 200, in );
#ifdef SPEC 
        if( sscanf( instring, "%" PRId64 " %" PRId64 " %" PRId64 , &t, &h, &c ) != 3 )
#else
        if( sscanf( instring, "%ld %ld %ld", &t, &h, &c ) != 3 )
#endif
                return -1;
        arc->id = actArc;
        arc->tail = &(node[t+net->n_trips]);
        arc->head = &(node[h]);
        arc->org_cost = (cost_t)c;
        arc->cost = (cost_t)c;
        arc->nextout = arc->tail->firstout;
        arc->tail->firstout = arc;
        arc->nextin = arc->head->firstin;
        arc->head->firstin = arc; 
    }
    arc = net->stop_arcs;
    if( net->stop_arcs != arc )
    {
        net->stop_arcs = arc;
        arc = net->arcs;
        for( net->m = 0; arc < net->stop_arcs; arc++ )
            (net->m)++;
        net->m_org = net->m;
    }
    fclose( in );
#ifdef DEBUG
    arc = net->arcs;
    for (i = 0; i< net->m; i++) {
      if (!arc->head) {
          printf("arc :%d is NULL\n", i);
      }
      arc++;
    }
#endif
    net->clustfile[0] = (char)0;
    for( i = 1; i <= net->n_trips; i++ )
    {
      arc = net->arcs + getArcPosition(net, 3 * i -1);
      arc->cost =
                (cost_t)((-2)*MAX(net->bigM,(LONG) BIGM));
      arc->org_cost =
                (cost_t)((-2)*(MAX(net->bigM,(LONG) BIGM)));
    }
    return 0;
}
//#include "mcf/treeup.c"
/**************************************************************************
TREEUP.C of ZIB optimizer MCF, SPEC version
Dres. Loebel, Borndoerfer & Weider GbR (LBW)
Churer Zeile 15, 12205 Berlin
Konrad-Zuse-Zentrum fuer Informationstechnik Berlin (ZIB)
Scientific Computing - Optimization
Takustr. 7, 14195 Berlin
This software was developed at ZIB Berlin. Maintenance and revisions 
solely on responsibility of LBW
Copyright (c) 1998-2000 ZIB.           
Copyright (c) 2000-2002 ZIB & Loebel.  
Copyright (c) 2003-2005 Loebel.
Copyright (c) 2006-2010 LBW.
**************************************************************************/
/*  LAST EDIT: Tue May 25 23:48:24 2010 by Loebel (opt0.zib.de)  */
/*  $Id: treeup.c,v 1.11 2010/05/25 21:58:44 bzfloebe Exp $  */
#include "mcf/treeup.h"
#ifdef _PROTO_
void update_tree( 
                 LONG cycle_ori,
                 LONG new_orientation,
                 flow_t delta,
                 flow_t new_flow,
                 node_t *iplus,
                 node_t *jplus,
                 node_t *iminus,
                 node_t *jminus,
                 node_t *w,
                 arc_t *bea,
                 cost_t sigma,
                 flow_t feas_tol
                )
#else
void update_tree( cycle_ori, new_orientation, delta, new_flow, 
                 iplus, jplus, iminus, jminus, w, bea, sigma, feas_tol )
     LONG cycle_ori;
     LONG new_orientation;
     flow_t delta; 
     flow_t new_flow;
     node_t *iplus, *jplus;
     node_t *iminus, *jminus;
     node_t *w;
     arc_t *bea;
     cost_t sigma; 
     flow_t feas_tol;
#endif
{
    arc_t    *basic_arc_temp;
    arc_t    *new_basic_arc;  
    node_t   *father;         
    node_t   *temp;           
    node_t   *new_pred;       
    LONG     orientation_temp;
    LONG     depth_temp;      
    LONG     depth_iminus;    
    LONG     new_depth;       
    flow_t   flow_temp;       
    /**/
    if( (bea->tail == jplus && sigma < 0) ||
        (bea->tail == iplus && sigma > 0) )
        sigma = ABS(sigma);
    else
        sigma = -(ABS(sigma));
    father = iminus;
    father->potential += sigma;
 RECURSION:
    temp = father->child;
    if( temp )
    {
    ITERATION:
        temp->potential += sigma;
        father = temp;
        goto RECURSION;
    }
 TEST:
    if( father == iminus )
        goto CONTINUE;
    temp = father->sibling;
    if( temp )
        goto ITERATION;
    father = father->pred;
    goto TEST;
 CONTINUE:
    /**/
    temp = iplus;
    father = temp->pred;
    new_depth = depth_iminus = iminus->depth;
    new_pred = jplus;
    new_basic_arc = bea;
    while( temp != jminus )
    {
        if( temp->sibling )
            temp->sibling->sibling_prev = temp->sibling_prev;
        if( temp->sibling_prev )
            temp->sibling_prev->sibling = temp->sibling;
        else father->child = temp->sibling;
        temp->pred = new_pred;
        temp->sibling = new_pred->child;
        if( temp->sibling )
            temp->sibling->sibling_prev = temp;
        new_pred->child = temp;
        temp->sibling_prev = 0;
        orientation_temp = !(temp->orientation); 
        if( orientation_temp == cycle_ori )
            flow_temp = temp->flow + delta;
        else
            flow_temp = temp->flow - delta;
        basic_arc_temp = temp->basic_arc;
        depth_temp = temp->depth;
        temp->orientation = new_orientation;
        temp->flow = new_flow;
        temp->basic_arc = new_basic_arc;
        temp->depth = new_depth;
        new_pred = temp;
        new_orientation = orientation_temp;
        new_flow = flow_temp;
        new_basic_arc = basic_arc_temp;
        new_depth = depth_iminus - depth_temp;      
        temp = father;
        father = temp->pred;
    } 
    if( delta > feas_tol )
    {
        for( temp = jminus; temp != w; temp = temp->pred )
        {
            temp->depth -= depth_iminus;
            if( temp->orientation != cycle_ori )
                temp->flow += delta;
            else
                temp->flow -= delta;
        }
        for( temp = jplus; temp != w; temp = temp->pred )
        {
            temp->depth += depth_iminus;
            if( temp->orientation == cycle_ori )
                temp->flow += delta;
            else
                temp->flow -= delta;
        }
    }
    else
    {
        for( temp = jminus; temp != w; temp = temp->pred )
            temp->depth -= depth_iminus;
        for( temp = jplus; temp != w; temp = temp->pred )
            temp->depth += depth_iminus;
    }
}
