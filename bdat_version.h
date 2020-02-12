#define _STR(x) #x
#define STR(x) _STR(x)

#define SPH_MAJOR 0
#define SPH_MINOR 1 
#define SPH_PATCH 0

#define BDAT_VERSION  STR(SPH_MAJOR.SPH_MINOR.SPH_PATCH)
