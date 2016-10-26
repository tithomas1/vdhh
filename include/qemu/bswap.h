#ifndef BSWAP_H
#define BSWAP_H


#include <sys/cdefs.h>
#include <sys/_types.h>
#include <machine/endian.h>

static uint64_t inline bswap64(uint64_t x)
{
    
    return  ( (x << 56) & 0xff00000000000000UL ) |
    ( (x << 40) & 0x00ff000000000000UL ) |
    ( (x << 24) & 0x0000ff0000000000UL ) |
    ( (x <<  8) & 0x000000ff00000000UL ) |
    ( (x >>  8) & 0x00000000ff000000UL ) |
    ( (x >> 24) & 0x0000000000ff0000UL ) |
    ( (x >> 40) & 0x000000000000ff00UL ) |
    ( (x >> 56) & 0x00000000000000ffUL );
    
}

static unsigned int inline bswap32 (unsigned int val)
{
    return (val & 0xff) << 24 |
    (val & 0xff00) << 8 |
    (val & 0xff0000) >> 8 |
    (val & 0xff000000) >> 24;
}

static unsigned short inline bswap16 (unsigned short val)
{
    return (val & 0xff) << 8 |
    (val & 0xff00) >> 8;
}

#define	htobe16(x)	bswap16((x))
#define	htobe32(x)	bswap32((x))
#define	htobe64(x)	bswap64((x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))

#define	be16toh(x)	bswap16((x))
#define	be32toh(x)	bswap32((x))
#define	be64toh(x)	bswap64((x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))

#define be32_to_cpu(x) be32toh(x)
#define cpu_to_be32(x) htobe32(x)
#define be16_to_cpu(x) be16toh(x)
#define cpu_to_be16(x) htobe16(x)
#define be64_to_cpu(x) be64toh(x)
#define cpu_to_be64(x) htobe64(x)


static __inline void
be16enc(void *pp, uint16_t u)
{
    unsigned char *p = (unsigned char *)pp;
    
    p[0] = (u >> 8) & 0xff;
    p[1] = u & 0xff;
}

static __inline uint16_t
be16dec(const void *pp)
{
    unsigned char const *p = (unsigned char const *)pp;
    
    return ((uint16_t) ((((uint32_t) p[0]) << 8) | ((uint32_t) p[1])));
}

static __inline void
be16decs(uint16_t *pp)
{
    *pp = be16dec(pp);
}


static __inline void
be32enc(void *pp, uint32_t u)
{
    unsigned char *p = (unsigned char *)pp;
    
    p[0] = (u >> 24) & 0xff;
    p[1] = (u >> 16) & 0xff;
    p[2] = (u >> 8) & 0xff;
    p[3] = u & 0xff;
}

static __inline uint32_t
be32dec(const void *pp)
{
    unsigned char const *p = (unsigned char const *)pp;
    
    return (uint32_t) ((((uint64_t) p[0]) << 24) |
                       (((uint64_t) p[1]) << 16) | (((uint64_t) p[2]) << 8) |
                       ((uint64_t) p[3]));
}

static __inline void
be32decs(uint32_t *pp)
{
    *pp = be32dec(pp);
}

static __inline void __unused
be64enc(void *pp, uint64_t u)
{
    uint8_t *p = pp;
    
    be32enc(p, u >> 32);
    be32enc(p + 4, u & 0xffffffffU);
}

static __inline uint64_t
be64dec(const void *pp)
{
    const uint8_t *p = pp;
    
    return (((uint64_t)be32dec(p) << 32) | be32dec(p + 4));
}

static __inline void
be64decs(uint64_t *pp)
{
    *pp = be64dec(pp);
}

static int inline ldl_le_p(const void *p)
{
    return *(uint32_t *)p;
}


typedef uint8_t jchar;
typedef uint32_t juint;

#define swab16(a) ( (((jchar)(a)) << 8) |                            \
(((jchar)(a)) >> 8)     )
#define swab32(a) ((juint)(                                          \
(((juint)(a)) << 24) |                          \
((((juint)(a)) & (juint)0x0000ff00UL) << 8) |    \
((((juint)(a)) & (juint)0x00ff0000UL) >> 8) |    \
(((juint)(a)) >> 24)     ))
#define swab64(a) ((julong)(                                                    \
(((julong)(a)) << 56) |                                    \
((((julong)(a)) & (julong)0x000000000000ff00ULL) << 40) |   \
((((julong)(a)) & (julong)0x0000000000ff0000ULL) << 24) |   \
((((julong)(a)) & (julong)0x00000000ff000000ULL) <<  8) |   \
((((julong)(a)) & (julong)0x000000ff00000000ULL) >>  8) |   \
((((julong)(a)) & (julong)0x0000ff0000000000ULL) >> 24) |   \
((((julong)(a)) & (julong)0x00ff000000000000ULL) >> 40) |   \
(((julong)(a)) >> 56)     ))


#define stq_be_p(x,y) be64enc(x,y)

#define be32_to_cpus(x) be32decs(x)
#define be64_to_cpus(x) be64decs(x)

#define be16_to_cpup(x) swab16(*(uint16_t *)x)
#define be32_to_cpup(x) swab32(*(uint32_t *)x)
#define le16_to_cpup(x) (*(uint16_t *)x)
#define le32_to_cpup(x) (*(uint32_t *)x)

#define cpu_to_be64s(x, y) be64enc(x, y)

#define cpu_to_be64w(x, y) be64enc((uint64_t*)x, y)
#define cpu_to_be32w(x, y) be32enc((uint64_t*)x, y)


#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)
#define cpu_to_le64w(x,y) (*(uint64_t *)x = y)

#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le64_to_cpu(x) (x)

#define le32_to_cpus(x) {}
#define le64_to_cpup(x) (*(uint64_t *)x)
#define le64_to_cpus(x) {}

static int inline ldl_be_p(const void *p)
{
    return bswap32(*(uint32_t *)p);
}


static int inline ldsw_be_p(const void *p)
{
    return bswap16(*(uint16_t *)p);
}

static uint64_t inline ldq_be_p(const void *p)
{
    return bswap64(*(uint64_t *)p);
}

static uint64_t inline ldq_le_p(const void *p)
{
    return (*(uint64_t *)p);
}

static inline int ldub_p(const void *p)
{
    return *(uint8_t *)p;
}

static uint16_t inline lduw_le_p(const void *p)
{
    return (*(uint16_t *)p);
}

static inline int ldsb_p(const void *p)
{
    return *(int8_t *)p;
}

static inline void stb_p(void *p, uint8_t v)
{
    *(uint8_t *)p = v;
}

static inline int lduw_be_p(const void *p)
{
    return (((uint8_t *)p)[0] << 8) | (((uint8_t *)p)[1]);
}

static inline void stl_be_p(void *ptr, int v)
{
    uint8_t *d = (uint8_t *) ptr;
    
    d[0] = v >> 24;
    d[1] = v >> 16;
    d[2] = v >> 8;
    d[3] = v;
}

static inline void stw_be_p(void *ptr, int v)
{
    uint8_t *d = (uint8_t *) ptr;
    d[0] = v >> 8;
    d[1] = v;
}

static inline void stl_le_p(void *ptr, int v)
{
    *(uint32_t *)ptr = v;
}

static inline void stq_le_p(void *ptr, uint64_t v)
{
    *(uint64_t *)ptr = v;
}

static inline void stw_le_p(void *ptr, uint16_t v)
{
    *(uint16_t *)ptr = v;
}

#endif