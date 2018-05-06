/*@null@*/
item *do_item_alloc(int node, char *key, const size_t nkey, const int flags, const rel_time_t exptime, const int nbytes, const uint32_t cur_hv);
void item_free(int node, item *it);

int  do_item_link(int node, item *it, const uint32_t hv);     /** may fail if transgresses limits */
void do_item_unlink(int node, item *it, const uint32_t hv);
void do_item_remove(int node, item *it);
void do_item_remove_free(int node, item *it);
void do_item_update(int node, item *it);   /** update LRU time to current and reposition */
int  do_item_replace(int node, item *it, item *new_it, const uint32_t hv);

item *do_item_get(int node, const char *key, const size_t nkey, const uint32_t hv);
item *do_item_touch(int node, const char *key, const size_t nkey, uint32_t exptime, const uint32_t hv);
