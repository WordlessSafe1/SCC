#ifndef _UNISTD_H_
# define _UNISTD_H_

void _exit(int status);
int _unlink(char *pathname);
int _access(char* path, int amode);
#define access	_access
#define exit	_exit
#define unlink	_unlink

#endif	// _UNISTD_H_