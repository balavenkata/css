#ifndef KITCHEN_H
#define KITCHEN_H

void *kitchen_thread_cb();

int ordershelf_to_max_size(SHELF shelf);

char *ordertemp_to_str(TEMP temp);

#endif //KITCHEN_H
