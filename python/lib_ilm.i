%module ilm

%include "typemaps.i"

%apply int *OUTPUT { int *sock };

%{
#include "ilm.h"
%}

int ilm_connect(int *sock);
int ilm_disconnect(int sock);
