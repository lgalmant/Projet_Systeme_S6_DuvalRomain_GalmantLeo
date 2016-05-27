#ifndef SHELL_FINAL_H_INCLUDED
#define SHELL_FINAL_H_INCLUDED
#include <unistd.h>

struct command
{
  char **argv;
};

struct processus
{
  char commande[100];
  pid_t PID;
};

void afficher(char **res, int nb);
void afficherP(struct command *cmd, int n);
void couper(char ch[], char **res);
int compterEspace(char saisie[500]);
int compterPipe(char saisie[500]);
void remplirHist(char saisie[]);
void hist(char **args);
void touch(char **args, int esp);
void cat(char **args, int esp);
void cp(char* path1, char* path2);
int copie(char *fic1, char *fic2);
int permissions(char *fic1, char *fic2);
int copieRep(char *rep1, char *rep2);
int estUnNombre(char* chaine);
int connectionPipe(int entree, int sortie, struct command *cmd);
int gestionPipes(int n, struct command *cmd);
void couperPipe(char *saisie, struct command *cmd);
int compterPipeEsp(char *saisie, int pipe);
void traitementCommande(char *saisie);
void traitementPipe(char *saisie);
void ajouterProc(pid_t pid, char commande[100]);
int monExec(char *nom, char** args);
int supprimerPID(pid_t pid);
void ps(pid_t pid);
void monWait(char **args);
void monKill(char **args);

int const BUFFER_SIZE = 1000;
int bg;
struct processus proc[20];
int nbProc;

#endif // SHELL_FINAL_H_INCLUDED
