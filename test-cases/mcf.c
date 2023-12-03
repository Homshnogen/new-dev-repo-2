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
#define PRId64 "ld" // does not get defined by headers/inttypes, for some reason

#include "mcf/mcf.c"
#include "mcf/implicit.c"
#include "mcf/mcfutil.c"
#include "mcf/output.c"
#include "mcf/pbeampp.c"
#include "mcf/pbla.c"
#include "mcf/pflowup.c"
#include "mcf/psimplex.c"
#include "mcf/pstart.c"
#include "mcf/readmin.c"
#include "mcf/treeup.c"