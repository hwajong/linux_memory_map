/*
 * OS Assignment #5
 *
 * @file person.h
 */

#ifndef __PERSON_H__
#define __PERSON_H__

#include <sys/types.h> /* for pid_t, size_t */
#include <string.h>    /* for strcmp() */

#define NOTIFY_MAX 16

typedef struct
{
  pid_t watchers[NOTIFY_MAX];

  char  name[64];
  int   age;
  int   gender; /* 1:male, 0:female */
  char  phone[24];
  char  homepage[80];
  char  twitter[80];
  char  facebook[80];
} Person;

#ifndef offsetof
#define offsetof(s,m) (size_t)&(((s *)0)->m)
#endif

static inline const char *
person_lookup_attr_with_offset (size_t offset)
{
  switch (offset)
    {
    case offsetof (Person, name):
      return "name";
    case offsetof (Person, age):
      return "age";
    case offsetof (Person, gender):
      return "gender";
    case offsetof (Person, phone):
      return "phone";
    case offsetof (Person, homepage):
      return "homepage";
    case offsetof (Person, twitter):
      return "twitter";
    case offsetof (Person, facebook):
      return "facebook";
    default:
      break;
    }

  return NULL;
}

static inline int
person_get_offset_of_attr (const char *attr_name)
{
  if (!strcmp (attr_name, "name"))
    return offsetof (Person, name);
  if (!strcmp (attr_name, "age"))
    return offsetof (Person, age);
  if (!strcmp (attr_name, "gender"))
    return offsetof (Person, gender);
  if (!strcmp (attr_name, "phone"))
    return offsetof (Person, phone);
  if (!strcmp (attr_name, "homepage"))
    return offsetof (Person, homepage);
  if (!strcmp (attr_name, "twitter"))
    return offsetof (Person, twitter);
  if (!strcmp (attr_name, "facebook"))
    return offsetof (Person, facebook);

  return -1;
}

static inline int
person_attr_is_integer (const char *attr_name)
{
  if (!strcmp (attr_name, "age") || !strcmp (attr_name, "gender"))
    return 1;

  return 0;
}

#endif /* __PERSON_H__ */
