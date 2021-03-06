/*
Classe:		client.c

Description:	GUI / connexion reseau / saisir le input du client

Auteurs:	Alain Sirois     SIRA15068305
		Philippe Mercure MERP27078708
Date:		18 juin 2011
Cours:		yyyyyyyyyy
Groupe:		30
Travail:	TP2
Professeur:	xxxxxxxxxx
*/


#include <curses.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>

#include "chaine.h"
#include "gui.h"
#include "config.h"



struct commande {
        char**  chaine;
        int     nbrToken;
        int     nsd;
};

struct commande cmd;
int socket_d;
int nom_usager_defini;

WINDOW * w_haut;
WINDOW * w_chat;
WINDOW * w_cmd;
WINDOW * w_info;
WINDOW * w_bas;

Chaine input;

Fenetre f_haut;
Fenetre f_bas;
Fenetre f_cmd;
Fenetre f_info;
Fenetre f_chat;

#define	marge_bas 1
#define f_haut_hauteur 3
#define f_cmd_hauteur 3
#define f_info_hauteur 5
#define f_bas_hauteur 1



/*
Reception des donnes du serveur
*/
int	recv_handler () {

	char	buffer[BUF_SIZE];
	int	n;

	n = recv (socket_d, buffer, BUF_SIZE, 0);
	if ( n > 0 ) {
		wprintw (w_chat, "%s\n", buffer );
		wrefresh (w_chat);
	}

	if ( ! strcmp(buffer, "Fermeture de la connexion client......") )
		return 0;

	if ( ! strcmp(buffer, "Votre nom est accepte!") )
		nom_usager_defini = 1;

	return 1;
}



/*
Decomposer la chaine de commande envoyer au serveur
*/
void decomposer_commande (char buffer[BUF_SIZE]) {
        char    copie[BUF_SIZE];
        sprintf (copie, "%s", buffer);

        cmd.chaine = (char**) malloc (1 * sizeof(char*));

        int nbrToken = 0;       
        int l = 0;
        int pos = 0;

        char*   pch;
        pch = strtok (copie, " ");
        while (pch != NULL) {
                nbrToken++;
                cmd.chaine = (char**) realloc (cmd.chaine, (nbrToken * sizeof(char*)) );

                if ( nbrToken < 3 ) {
                        l = (int) strlen (pch);
                        pos = pos + l + 1;

                        cmd.chaine[nbrToken-1] = (char*) malloc ( (l + 1) * sizeof(char));
                        sprintf (cmd.chaine[nbrToken-1], "%s", pch);

                } else {
                        cmd.chaine[nbrToken-1] = (char*) malloc ( ((int)strlen(&buffer[pos])+1) * sizeof(char));
                        sprintf (cmd.chaine[nbrToken-1], "%s", &buffer[pos]);
                        break;
                }       

                pch = strtok (NULL, " ");
        }       

        cmd.nbrToken = nbrToken;
}



/*
Traitement du standard input
*/
void key_handler () {

	int ch = wgetch(w_bas);  
	int longueur = chaineLongueur(input);

	if ( ch != '\n' && ch != ERR && ch!= 127 ) { // AJOUTER CHAR
		chaineAjouter( input, ch );

		if ( longueur > COLS ) {
			mvwprintw(w_bas, 0, 0, "%s", &chaineValeur(input)[longueur-COLS] );
		}
			

	} else if ( ch == 127 ) { // BACKSPACE
		if ( chaineLongueur(input) > 0 ) {
			chaineEnlever(input);
			delwin(w_bas);
			w_bas = create_newwin_no_border( f_bas );
			
			if ( longueur > COLS ) {
				mvwprintw(w_bas, 0, 0, "%s", &chaineValeur(input)[longueur-COLS] );
			} else {
				mvwprintw( w_bas, 0, 0, "%s", chaineValeur(input) );
			}

		} else {
			delwin(w_bas);
			w_bas = create_newwin_no_border( f_bas );
			wmove( w_bas, 0, 0 );
		}

	} else if ( ch == '\n' ) { // ENTER
		delwin(w_cmd);
		w_cmd = create_newwin_with_border( f_cmd );
		mvwprintw(w_cmd, 1, 1, "%s", chaineValeur(input) );

		delwin(w_bas);
		w_bas = create_newwin_no_border( f_bas );
		wmove( w_bas, 0, 0 );

		char buffer[BUF_SIZE];
		sprintf ( buffer, "%s", chaineValeur(input) );

		decomposer_commande (buffer);
		if ( nom_usager_defini == 1 && !strcmp("/nom", cmd.chaine[0]) ) {
			wprintw (w_chat, "Erreur! Vous ne pouvez pas changer de nom!\n");
		
		} else if ( nom_usager_defini == 0 && strcmp("/nom", cmd.chaine[0]) ) {
			wprintw (w_chat, "Erreur! Vous devez definir un nom d'usager tout d'abord!\n");

		} else {
			int n = send (socket_d, buffer, strlen(buffer)+1, 0);
			if ( n < 0 )
				wprintw(w_chat, "Erreur lors de l'envoi\n");
		}
			
		chaineSupprime( input );
		input = chaineCreerVide( COLS );
	}

	wmove( w_bas, 0, chaineLongueur(input) );
	wrefresh(w_chat);
	wrefresh(w_cmd);
	wrefresh(w_bas);
}



/*
Modifier une socket comme non bloquante
*/
void setnonblocking (int sock) {
	int opts;

	opts = fcntl(sock,F_GETFL);
	if (opts < 0) {
		perror("erreur fcntl(F_GETFL)");
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock,F_SETFL,opts) < 0) {
		perror("erreur fcntl(F_SETFL)");
	}
}



/*
MAIN
Envoyer et recevoir des donnes
Gere les fenetres du GUI
*/
int main (int argc, char* argv[]) {

	if ( argc < 2 ) {
		printf ("Usage: %s PORT\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	initscr();	// Start curses mode
	cbreak();	// Line buffering disabled, Pass on everty thing to me

	//my_win = create_newwin(height, width, starty, startx);
	f_haut	= definirFenetre( f_haut_hauteur, COLS, 0, 0 );
	f_bas	= definirFenetre( f_bas_hauteur, COLS, (LINES - f_bas_hauteur - marge_bas), 0 );
	f_info	= definirFenetre( f_info_hauteur, COLS, (LINES - donnerHauteur(f_bas) - f_info_hauteur - marge_bas), 0 );
	f_cmd 	= definirFenetre( f_cmd_hauteur, COLS, (LINES - donnerHauteur(f_bas) - donnerHauteur(f_info) - marge_bas - f_cmd_hauteur), 0);
	f_chat	= definirFenetre( (LINES - donnerHauteur(f_haut) - donnerHauteur(f_cmd) - donnerHauteur(f_info) - donnerHauteur(f_bas) - marge_bas), COLS, donnerHauteur(f_haut), 0 );

	refresh();
	w_haut	= create_newwin_with_border( f_haut );
	w_bas	= create_newwin_no_border( f_bas );
	w_info	= create_newwin_with_border( f_info );
	w_cmd	= create_newwin_with_border( f_cmd );
	w_chat	= create_newwin_no_border( f_chat );

	scrollok( w_chat, 1 );
	wsetscrreg( w_chat, donnerStarty(f_chat), donnerHauteur(f_chat) );
	wtimeout(w_bas, 500);

	mvwprintw(w_haut, 1, 1, "CHAT CLIENT");
	mvwprintw(w_cmd, 1, 1, "");
	mvwprintw(w_info, 1, 1, "/nom usager\t/mp usager msg\t/creer   groupe type\t/info  groupe\t\t/accept  usager groupe");
	mvwprintw(w_info, 2, 1, "\t\t/mg groupe msg\t/joindre groupe\t\t/liste usagers\t\t/refuser usager groupe");
	mvwprintw(w_info, 3, 1, "/quitter\t\t\t/byebye  groupe\t\t/liste groupes\t\t/stats   groupe");
	wmove( w_bas, 0, 0 );
	wrefresh(w_haut);
	wrefresh(w_info);
	wrefresh(w_bas);
	wrefresh(w_cmd);

	
	struct sockaddr_in	serveur;
	struct hostent*		hp;

	socket_d = socket (AF_INET, SOCK_STREAM, 0);
	if (socket_d < 0) {
		endwin();
		printf("Erreur lors de la création de la socket !\n");
		return 1;
	}
	setnonblocking (socket_d);

	hp = gethostbyname("localhost");
	if (hp==0) {
		endwin();
		close (socket_d);
		printf("Hôte inconnu!\n");
		return 2;
	}

	serveur.sin_family = AF_INET;
	serveur.sin_port = htons(atoi(argv[1]));
	bcopy((char *)hp->h_addr, (char *)&serveur.sin_addr, hp->h_length);

	if ( connect(socket_d,(struct sockaddr *)&serveur,sizeof(struct sockaddr_in)) < 0 ) {
		endwin();
		close (socket_d);
		printf("Erreur lors de la création d'une nouvelle connexion !\n");
		return 3;
	}


	nom_usager_defini = 0;

	input = chaineCreerVide( COLS );
	while ( 1 ) {
		key_handler();

		if ( ! recv_handler() )
			break;
	}	

	endwin ();
	close (socket_d);
	return 0;
}
