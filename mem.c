/* On inclut l'interface publique */
#include "mem.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* Définition de l'alignement recherché
 * Avec gcc, on peut utiliser __BIGGEST_ALIGNMENT__
 * sinon, on utilise 16 qui conviendra aux plateformes qu'on cible
 */
#ifdef __BIGGEST_ALIGNMENT__
#define ALIGNMENT __BIGGEST_ALIGNMENT__
#else
#define ALIGNMENT 16
#endif

/* structure placée au début de la zone de l'allocateur

   Elle contient toutes les variables globales nécessaires au
   fonctionnement de l'allocateur

   Elle peut bien évidemment être complétée
*/
//metadonnée de toute la zone mémoire 
struct allocator_header {
        size_t memory_size; //taille de la mémoire
	mem_fit_function_t *fit; //pointeur sur la fonction de recherche 
	//d'une zone libre à allouer
	struct fb* liste_chainee; //pointeur sur la première zone libre
}; 


/* La seule variable globale autorisée
 * On trouve à cette adresse le début de la zone à gérer
 * (et une structure 'struct allocator_header)
 */
static void* memory_addr;

static inline void *get_system_memory_addr() {
	return memory_addr;
}

static inline struct allocator_header *get_header() {
	struct allocator_header *h;
	h = get_system_memory_addr();
	return h;
}

static inline size_t get_system_memory_size() {
	return get_header()->memory_size;
}

//metadonnée d'une zone libre
struct fb {
	size_t size; //sa taille
	struct fb* next; //un pointeur sur la prochaine zone libre
	struct fb* previous; //un pointeur sur la précédante zone libre
};


/*
*Initialisation de la liste des zones libres avec une seule 
*zone correspondant à l'ensemble de la mémoire située en 'mem'
*et de taille 'taille' non utilisée
*lorsqu'on appelle 'men_init' on réinitialise donc toute la 
*mémoire
*/
void mem_init(void* mem, size_t taille)
{
	 if (taille > 0){
		memory_addr = mem; //on définit l'adresse de la mémoire
		assert(mem == get_system_memory_addr());

		long s=sizeof(struct allocator_header); // on crée un long pour 
		//plus de facilitée de lecture (long car sizeof renvoie toujours 
		//un long)

		struct allocator_header* header = (struct allocator_header*)mem; 
		//on crée le header en lui donnant son adresse (il faut ici caster 
		//car men est un void*)
		header->memory_size = taille; //on lui definit alors sa taille
		header->fit = mem_fit_first; //sa fonction
		header->liste_chainee = mem+s; //l'adresse de la première zone 
		//libre (c'est-à-dire l'adresse de départ + la taille d'une 
		//structure d'allocateur mémoire(=24))
		assert(taille == get_system_memory_size());
		
		struct fb* ffb = mem+s;//de même on crée la première zone libre 
		//en lui donnant son adresse 
		ffb->size = taille-s; //on lui definit alors sa taille
		ffb->next = NULL; //il n'y a pas de deuxième zone libre au moment de 
		//l'initialisation
		ffb->previous = (struct fb*) header; //par défaut on pose que 
		//la première zone libre est le header ce qui est faux pour ne 
		//pas avoir de problème d'allocation dans mem_alloc
		
		/* On vérifie avec les asserts qu'on a bien enregistré les infos 
		 * et qu'on sera capable de les récupérer par la suite
		 */
		 mem_fit(&mem_fit_first);
	 }
}

void mem_show(void (*print)(void *, size_t, int)) {
	void* currentAdress = get_system_memory_addr() + 24; //on note 
	//currentAdress le pointeur qui pointe sur la zone que l'on étudie
	//et on lui ajoute 24 (=la taille du header)
	struct fb* currentFBAdress = get_header()->liste_chainee; //on note 
	//currentFBAdress le pointeur qui parcours la liste chaînée de zones 
	//libres (on l'initialise alors comme le pointeur du header qui 
	//pointe sur la première zone libre)
	while(currentFBAdress != NULL){
		if (currentAdress == currentFBAdress){
			print(currentAdress,currentFBAdress->size,1);
			currentAdress = currentAdress + currentFBAdress->size;
			currentFBAdress = currentFBAdress->next;
		}
		else {
			struct fb* currentOBAdress = (struct fb*) currentAdress;
			print(currentOBAdress, currentOBAdress->size,0);
			currentAdress = currentAdress + currentOBAdress->size;
		}
	}
	//attention premier bloc pas forcement vide donc utiliser adresse 
	//definie dans alloc_header
	//while (currentFBAdress != NULL) { //simplification de la condition
	//	if (currentFBAdress->next == currentFBAdress + currentFBAdress->size )
	//{//on peut pas faire ça car c'est imposibble (ou alors ça serait 2 fb 
	//à côté)
	//		struct ob* zoneoccupee = (struct ob*) 
	//currentFBAdress+currentFBAdress->size;
	//		print(zoneoccupee,zoneoccupee->size,0);
	//	} else {
	//		print(currentFBAdress,currentFBAdress->size,1);
	//	}
	//	currentFBAdress = currentFBAdress->next;
	//}
}


void mem_fit(mem_fit_function_t *f) {
	get_header()->fit = f;
}
//transformation de zone occupée en zone libre + chainer en zone libre
//après voir si on peut regrouper les zones libres


/*
*Cette procédure reçoit en paramètre la taille (size) de la 
*à allouer. Elle retourne un pointeur vers la zone allouée
*et (NULL) en cas d'allocation impossible
*/
void *mem_alloc(size_t taille) {
	//__attribute__((unused)); 
	//juste pour que gcc compile ce squelette avec -Werror 
	struct fb* currentFBAdress = get_header()->liste_chainee;
	struct fb* fit_first = mem_fit_first(currentFBAdress, taille);
	//on a bien vérifié avec mem_fit_first que fit_first->size est 
	//plus grande que taille
	if (fit_first != 0){
		if (fit_first->size == taille){
			//il faut allouer la mémoire et changer les next et previous
			struct fb* precedent = fit_first->previous;
			struct fb* prochain = fit_first->next;
			precedent-> next = prochain;
		}
		else{//cas où fit_first->size > taille
			if(fit_first->size - taille < sizeof(struct fb*)){
				//la taille qui restera de la zone libre 
				//après avoir l'alloué sera trop petite pour accueillir 
				//de nouvelles données, on doit donc allouer la 
				//zone libre en entier.
				struct fb* precedent = fit_first->previous;
				struct fb* prochain = fit_first->next;
				precedent->next = prochain;
			}
			else{
				struct fb* newcurrentFBAdress = fit_first + taille;  
				struct fb* precedent = fit_first->previous; 
				precedent->next = newcurrentFBAdress;
				newcurrentFBAdress->size = fit_first->size - taille;
				newcurrentFBAdress->next = fit_first->next;
				newcurrentFBAdress->previous = fit_first->previous;
			}
		//struct fb* fb = get_header()->fit(NULL,0)
		}
		return fit_first;
	}
	return NULL;
}

/*
*Cette procédure reçoit en paramètre l'adresse de la zone 
*occupée. La taille de la zone est récupéré en début de zone.
*La fonction met à jour la liste des zones libres avec fusion 
*des zones libres contiguës si le cas se présente.
*/

void mem_free(void* mem) {
	struct fb* currentFBAdress = get_header()->liste_chainee;
	if (mem > get_system_memory_addr() + 24 && mem < get_system_memory_addr() + get_system_memory_size()){ //on vérifie que la zone n'est pas dans le header
		struct fb* zoneoccupee = (struct fb*) mem;
		while (currentFBAdress < zoneoccupee){
			currentFBAdress = currentFBAdress->next;
		}
		struct fb* precedent = currentFBAdress->previous;
		
		if (currentFBAdress - zoneoccupee->size == 0){
			if (precedent + precedent->size == zoneoccupee){
			//cela veut dire que la zone avant la zone occupée 
			//est libre mais aussi que celle après la zone
			//occupée est libre aussi, on peut donc les lier
			struct fb* newcurrentFBAdress = precedent;
			newcurrentFBAdress->size = precedent->size + zoneoccupee->size + currentFBAdress->size;
			newcurrentFBAdress->next = currentFBAdress->next;
			newcurrentFBAdress->previous = precedent->previous;
			}
			else {
				//cela veut dire que la zone après la zoneoccupée
				//est libre, (mais pas celle avant)
				//on peut donc les lier
				struct fb* newcurrentFBAdress = zoneoccupee;
				precedent->next = newcurrentFBAdress;
				newcurrentFBAdress->size = zoneoccupee->size + currentFBAdress->size;
				newcurrentFBAdress->next = currentFBAdress->next;
				newcurrentFBAdress->previous = currentFBAdress->previous;
			}
		}
		else {
			if (precedent + precedent->size == zoneoccupee){
				//cela veut dire que la zone avant la zone occupée 
				//est libre, on peut donc les lier
				struct fb* newcurrentFBAdress = precedent;
				newcurrentFBAdress->size = precedent->size + zoneoccupee->size;
				newcurrentFBAdress->next = precedent->next;
				newcurrentFBAdress->previous = precedent->previous;
			}
			else {
				//la zone occupee n'a pas de zone libre à côté d'elle
				//on crée alors une nouvelle zone libre et on la rajoute
				//dans la liste chainée
				struct fb* newcurrentFBAdress = zoneoccupee;
				precedent->next = newcurrentFBAdress;
				currentFBAdress->previous = newcurrentFBAdress;
				newcurrentFBAdress->size = zoneoccupee->size;
				newcurrentFBAdress->next = currentFBAdress;
				newcurrentFBAdress->previous = precedent;
			}
		}
	}
}
	


/*
*Permet de choisir la fonction de recherche.
*/

struct fb* mem_fit_first(struct fb *list, size_t size) {
	while (list != NULL){
		if (size <= list->size){
			return list;
		}
		else{
			list= list->next;
			}
	}
	return NULL;
}	


/* Fonction à faire dans un second temps
 * - utilisée par realloc() dans malloc_stub.c
 * - nécessaire pour remplacer l'allocateur de la libc
 * - donc nécessaire pour 'make test_ls'
 * Lire malloc_stub.c pour comprendre son utilisation
 * (ou en discuter avec l'enseignant)
 */
size_t mem_get_size(void *zone) {
	/* zone est une adresse qui a été retournée par mem_alloc() */

	/* la valeur retournée doit être la taille maximale que
	 * l'utilisateur peut utiliser dans cette zone */
	return 0;
}

/* Fonctions facultatives
 * autres stratégies d'allocation
 */
struct fb* mem_fit_best(struct fb *list, size_t size) {
	return NULL;
}

struct fb* mem_fit_worst(struct fb *list, size_t size) {
	return NULL;
}
