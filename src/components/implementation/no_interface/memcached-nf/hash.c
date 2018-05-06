/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "memcached.h"
#include "jenkins_hash.h"

int
hash_init(enum hashfunc_type type)
{
    switch(type) {
        case JENKINS_HASH:
            hash = jenkins_hash;
            break;
        default:
            return -1;
    }
    return 0;
}
