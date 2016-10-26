#ifndef TLS_H
#define TLS_H

#define DEFINE_TLS(type, name) __typeof__(type) veert__##name
#define DECLARE_TLS(type, name) extern __typeof__(type) veert__##name
#define tls_var(name) veert__##name

#endif
