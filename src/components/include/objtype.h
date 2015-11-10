/**
   Jiguo: the mapping between the type of the object which needs to be
   recovered and the component name/id which needs to be reflected
   (for reflection in c^3 only)
**/

#ifndef OBJTYPE_H
#define OBJTYPE_H

//#define REFLECTION  // do the reflection on the bottom level component interface

#define SPD_SCHED 20
#define SPD_MM    21
#define SPD_UNIQ  22

enum {
	REFLECT_OBJ_THREAD = 0,
	REFLECT_OBJ_PAGE,
	REFLECT_OBJ_FS,
	REFLECT_OBJ_KERNEL,
	REFLECT_OBJ_MAX
};

#endif /* OBJTYPE_H */
