#ifndef _CIO_H_
#define _CIO_H_

#define NULL ((void *)0)

typedef char *string;

int print(const char *format, ...);
string input(const char *const String, ...);

char input_char(const char *const String, ...);
int input_int(const char *const String, ...);
long input_long(const char *const String, ...);
long long input_long_long(const char *const String, ...);
double input_double(const char *const String, ...);

#endif /* _CIO_H_ */