#include "crc.h"


unsigned int ui_hash(const std::string &data_blk_ptr, unsigned int hash_size)
{
	register unsigned int i, j;
	register unsigned int crc_accum = -1;
	for (const char &element : data_blk_ptr)
	{
		i = ((int)(crc_accum >> 24) ^ element) & 0xFF; // XOR with current byte and mask to get the lookup table index
		crc_accum = (crc_accum << 8) ^ crc_table[i]; // Update the CRC using the lookup table.
	}
	crc_accum = ~crc_accum;

	return crc_accum % hash_size;
}

unsigned int hash(const char* data_blk_ptr, unsigned int hash_size)
{
	register unsigned int i, j;
	register unsigned int crc_accum = -1;
	for (j = 0; *data_blk_ptr != '\0'; j++)
	{
		i = ((int)(crc_accum >> 24) ^ *data_blk_ptr++) & 0xFF; // XOR with current byte and mask to get the lookup table index
		crc_accum = (crc_accum << 8) ^ crc_table[i]; // Update the CRC using the lookup table.
	}
	crc_accum = ~crc_accum;

	return crc_accum % hash_size;
}

 std::shared_ptr<ui_nlist> ui_lookup(const std::string &dm)
{
	std::shared_ptr<ui_nlist> np;
	for (np = ui_hashtab[ui_hash(dm, HASHSIZE)]; np != NULL; np = np->next)
		if (dm == np->defn.ui_domain)
			return np; /* found */
	return NULL; /* not found */
}

struct nlist* lookup(const char* s)
{
	struct nlist* np;
	for (np = hashtab[hash(s, HASHSIZE)]; np != NULL; np = np->next)
		if (strcmp(s, np->defn.domain) == 0)
			return np; /* found */
	return NULL; /* not found */
}

void ui_install(UICredential ui_map)
{
	std::shared_ptr<ui_nlist> np;
	unsigned hashval;
	if ((np = ui_lookup(ui_map.ui_domain)) == NULL) { /* not found */
		np = std::make_shared<ui_nlist>();

		np->defn.ui_domain = ui_map.ui_domain;
		np->defn.ui_login = ui_map.ui_login;
		np->defn.ui_password = ui_map.ui_password;

		hashval = ui_hash(ui_map.ui_domain, HASHSIZE);
		np->next = ui_hashtab[hashval];
		ui_hashtab[hashval] = np;
		spdlog::debug("added new element to hash table with domain argument: {}  and  {} key", ui_map.ui_domain, hashval);
	}
	else
	{
		spdlog::error("ui_map with domain argument: {} already exist in hash table",ui_map.ui_domain);
	}
}

struct nlist* install(IUIAutomationMap ui_map)
{
	struct nlist* np;
	unsigned hashval;
	//TODO:provide linked list along buckets, and remove sequentially before delete
	/* only one element in bucket ( not possible to attach next struct (no linked list)*/
	if ((np = lookup(ui_map.domain)) == NULL) { /* not found */
		np = (struct nlist*)malloc(sizeof(*np));
		if (np == NULL || (np->defn.data[0] = ui_map.data[0]) == NULL || (np->defn.data[1] = ui_map.data[1]) == NULL || (np->defn.data[2] = ui_map.data[2]) == NULL)
			return NULL;
		hashval = hash(ui_map.domain, HASHSIZE);
		np->next = hashtab[hashval];
		hashtab[hashval] = np;
	}
	else /* already there */
		return NULL;
	return np;
}

void ui_init_dict(const std::vector<UICredential> &credentials)
{
	for (const auto & entry : credentials)
	{
		ui_install(entry);
	}
}

void init_dict(IUIAutomationMap ui_map[])
{
	for (size_t i = 0; ui_map[i].domain != nullptr; i++)
	{
		install(ui_map[i]);
	}
}

void release_dict()
{
	for (size_t i = 0; i < HASHSIZE; i++)
	{
		free(hashtab[i]);
	}
}