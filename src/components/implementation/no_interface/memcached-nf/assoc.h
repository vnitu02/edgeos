/* associative array */

#define hashsize(n) ((unsigned long int)1<<(n))
#define hashmask(n) (hashsize(n)-1)

extern unsigned int hashpower;

void assoc_init(int node, const int hashpower_init);
item *assoc_find(int node, const char *key, const size_t nkey, const uint32_t hv);
int assoc_insert(int node, item *item, const uint32_t hv);
void assoc_delete(int node, const char *key, const size_t nkey, const uint32_t hv);
void assoc_replace(int node, item *old, item *new, const uint32_t hv);
void assoc_flush_tbl(void);
void assoc_flush_opt(const uint32_t hv, item *it);
void assoc_flush(const uint32_t hv);
extern unsigned int hashpower;
