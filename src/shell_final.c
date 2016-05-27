#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <signal.h>

#include "shell_final.h"

int main()
{

    char *hn = malloc(1000*sizeof(char));
    char *r = malloc(1000*sizeof(char));
    char *tout = malloc(1000*sizeof(char));
    nbProc = 0;

    char saisie[500];

    do{
        getcwd(r, 1000);

        sprintf(tout, "%s%s%s%s%s%s", "login", "@", hn, ":", r, ">");
        fprintf(stdout, "%s", tout);
        bg = 0;
        if(fgets(saisie, sizeof saisie, stdin) != 0){
            char *entry = NULL;
            entry = strchr(saisie, '\n');
            if(entry != NULL)
                *entry = '\0';

            if(strchr(saisie, '|') != 0){

                traitementPipe(saisie);
            }
            else{

                traitementCommande(saisie);
            }


            remplirHist(saisie);

        }
        else
            break;

    }while(1);
    printf("\n");
    free(hn);
    free(r);
    free(tout);

    return 0;
}

// Remplace execvp
int monExec(char *nom, char** args){

    char* path, *premier, *dernier;
    const int TAILLE_MAX_NOM = 1000;
    char nomComplet[TAILLE_MAX_NOM + 1];
    int tailleNom, taille, pasAcces;

    pasAcces = 0;

    if(strchr(nom, '/') != NULL){
        execv(nom, args);
    }

    path = getenv("PATH");

    tailleNom = strlen(nom);

    for(premier = path; ; premier = dernier+1){
        for(dernier = premier ; (*dernier != 0) && (*dernier != ':'); dernier ++){

        }
        taille = dernier - premier;
        if((taille + tailleNom + 2) >= TAILLE_MAX_NOM){
            continue;
        }
        strncpy(nomComplet, premier, taille);
        if (dernier[-1] != '/') {
            nomComplet[taille] = '/';
            taille++;
        }
        strcpy(nomComplet + taille, nom);
        execv(nomComplet, args);

        if (errno == EACCES) {
            pasAcces = 1;
        } else if (errno != ENOENT) {
            break;
        }
        if (*dernier == 0) {

            if (pasAcces) {
                errno = EACCES;
            }
            break;
        }

    }

    return -1;
}

// Traitement si la commande dispose d'un pipe
void traitementPipe(char *saisie){
    int p;
    int status;
    int nbP = compterPipe(saisie)+1;
    int oldStdout;
    printf("nbP = %d, saisie = %s\n", nbP, saisie);
    struct command *cmd = malloc(nbP * sizeof(struct command));
    for(int i=0;i<nbP;i++){
        p = compterPipeEsp(saisie, i)+1;
        cmd[i].argv = malloc(p * sizeof(char*));
        for(int j=0;j<p;j++){
            cmd[i].argv[j] = (char*)malloc(500*sizeof(char));
        }
    }
    couperPipe(saisie, cmd);
    pid_t pid;
    pid = fork();

    if(pid == 0){

        gestionPipes(nbP, cmd);
        exit(0);

    }
    else{
        waitpid(pid, &status, 0);
    }


    for(int i=0;i<nbP;i++){
        p = compterPipeEsp(saisie, i);
        for(int j=0;j<p;j++){
            free(cmd[i].argv[j]);
        }
        free(cmd[i].argv);
    }
    free(cmd);

}

// Traitement si la commande ne dispose pas de pipe (commande simple)
void traitementCommande(char *saisie){
    int status;
    char **args;
    char **argsT;
    int fichier;
    int redir = 0;
    int oldStdout;
    int esp = compterEspace(saisie)+1;

    args = malloc(esp * sizeof(char*));
    for(int i=0;i<esp;i++){
        args[i] = (char*)malloc(1000*sizeof(char));
    }
    couper(saisie, args);

            //printf("args[0]=%s\n", args[0]);

    if(esp > 2){
        if(!strcmp(args[esp-2], ">")){

            fichier = open(args[esp-1], O_CREAT | O_WRONLY | O_TRUNC);
            oldStdout = dup(STDOUT_FILENO);

            free(args[esp-1]);
            free(args[esp-2]);
            esp -= 2;

            dup2(fichier,STDOUT_FILENO);
            redir = 1;
        }
        else if(!strcmp(args[esp-2], ">>")){
            fichier = open(args[esp-1], O_CREAT | O_APPEND | O_WRONLY);
            oldStdout = dup(STDOUT_FILENO);
            dup2(fichier,STDOUT_FILENO);
            free(args[esp-1]);
            free(args[esp-2]);
            esp -= 2;
            redir = 1;
        }
    }
    if(strchr(saisie, '&') != NULL){
        bg = 1;
        free(args[esp-1]);
        esp--;
    }

    argsT = malloc(esp * sizeof(char)*500);
    for(int i=0;i<esp;i++){
        argsT[i] = (char*)malloc(1000*sizeof(char));
        strcpy(argsT[i], args[i]);
    }

    if(!strcmp(args[0], "exit")){
        exit(0);
    }

    else if(!strcmp(args[0], "cd")){
        if(args[1]==NULL || !strcmp(args[1], "~")){
            const char *const dir = getenv("HOME");
            chdir(dir);
        }
        else{
            const char *const dir = args[1];

            chdir(dir);
        }

    }
    else if(!strcmp(args[0], "wait")){
        monWait(args);
    }
    else if(!strcmp(args[0], "kill")){
        monKill(args);
    }
    pid_t pidActu = getpid();

    pid_t pid;
    pid = fork();

    if(pid == 0){
        ajouterProc(getpid(), args[0]);
        if(!strcmp(args[0], "history")){
            hist(args);
        }
        else if(!strcmp(args[0], "touch")){
            touch(args, esp);
        }
        else if(!strcmp(args[0], "cat")){
            cat(args, esp);
        }
        /*else if(!strcmp(args[0], "ps")){
            ps(pidActu);
        }*/
        else if(!strcmp(args[0], "cp")){
            cp(args[1], args[2]);
        }
        else if(!strcmp(args[0], "quit")){
            printf("Cette commande n'existe malheureusement pas\n");
        }
        else{
            monExec(argsT[0], argsT);
        }
        supprimerPID(getpid());
        exit(0);

    }
    else if(bg != 1){
        waitpid(pid, &status, 0);
    }


    if(redir){
        dup2(oldStdout, STDOUT_FILENO);
        close(fichier);
        close(oldStdout);

        redir = 0;

    }
    for(int i=0;i<esp;i++){
        free(args[i]);
    }
    free(args);

    for(int i=0;i<esp;i++){
        free(argsT[i]);
    }
    free(argsT);
}

// Permet de tuer le processus en argument
void monKill(char **args){
    if(args[1]!=NULL){
        int status;
        int t = atoi(args[1]);
        pid_t pid = t;
        kill(t, SIGUSR1);
    }
    else{
        fprintf(stderr, "Il manque un argument\n");
    }

}

// Permet d'attentre le processus en argument
void monWait(char **args){
    if(args[1]!=NULL){
        int status;
        int t = atoi(args[1]);
        pid_t pid = t;
        waitpid(0, &status, 0);
    }
    else{
        fprintf(stderr, "Il manque un argument\n");
    }

}

// Ebauche de la fonction ps
int supprimerPID(pid_t pid){
  int p = pid;
  FILE *F, *FW;
  char s[100];
  int n1;


  if (NULL == (F = fopen ("processus", "r")))
    return -1;
  if (NULL == (FW = fopen ("processusTemp", "w")))
    return -1;

  while (fscanf (F, "%d %s", &n1, s) == 2)
    if (p != n1);
      fprintf (FW, "%d %s\n", n1, s);
  fclose (F);
  fclose (FW);

  remove("processus");
  rename("processusTemp", "processus");
}

// Aurait permis d'afficher les processus en cours
void ps(pid_t pid){
    FILE* fichier = NULL;
    char chaine[10000] = "";

    fichier = fopen("processus", "r");
    printf("PID     CMD nbProc = %d\n", nbProc);
    printf("%d    Bash\n", pid);
    while (fgets(chaine, 10000, fichier) != NULL){
        printf("%s", chaine);
    }
    fclose(fichier);
    /*printf("%d    Bash\n", pid);
    for(int i=0;i<nbProc; i++){
        printf("%d    %s\n", proc[i].PID, proc[i].commande);
    }*/

}

// Ebauche de la fonction ps
void ajouterProc(pid_t pid, char commande[100]){
    /*struct processus p;
    p.PID = pid;

    strcpy(p.commande, commande);
    proc[nbProc] = p;
    printf("%d\n", nbProc);
    nbProc++;*/

    FILE* fichier = NULL;
    fichier = fopen("processus", "a");
    if (fichier != NULL)
    {

        fprintf(fichier, "%d   %s\n", pid, commande);
        fclose(fichier);
    }
}

// Permet d'allouer la mémoire si la commande dispose d'un pipe
int compterPipeEsp(char *saisie, int pipe){
    int cPipe = 0;
    int esp=0;
    int i=0;
    while(cPipe<pipe)
    {
        if(saisie[i] == '|'){
            cPipe++;
        }
        i++;
    }

    while(i<strlen(saisie) && saisie[i] != '|'){
        if(saisie[i] == ' ' && saisie[i-1] != '|' && saisie[i+1] != '|'){
            esp++;
        }
        i++;
    }
    return esp;
}

void afficherP(struct command *cmd, int n){
    for(int i;i<n;i++){
        for(int j=0;j<20;j++){
            printf("%s ", cmd[i].argv[j]);
        }
        printf("\n");
    }
}

// Permet de découper une commande munie d'un pipe dans le format souhaité
void couperPipe(char *saisie, struct command *cmd){
    int car = 0;
    int mot = 0;
    int comm = 0;
    for(int i = 0;i<strlen(saisie);i++){

        if(saisie[i] != ' ' && saisie[i] != '|'){
            cmd[comm].argv[mot][car] = saisie[i];
            //printf("cmd[%d].argv[%d][%d] = saisie[%d] : %c\n\n", comm, mot, car, i, saisie[i]);
            car++;
        }
        else if(saisie[i] == ' ' && saisie[i-1] != '|' && saisie[i+1] != '|'){
            cmd[comm].argv[mot][car] = '\0';
            mot++;
            car = 0;
        }
        else if(saisie[i] == '|'){
            //strcpy(cmd[comm].argv[mot], 0);
            comm++;
            mot = 0;
            car = 0;
        }

        cmd[comm].argv[mot][car] = '\0';
        //i++;

    }
}

// Gère les redirections des pipes
int connectionPipe(int entree, int sortie, struct command *cmd){
    pid_t pid;

      if ((pid = fork ()) == 0)
        {
          if (entree != 0)
            {
              dup2 (entree, 0);
              close (entree);
            }

          if (sortie != 1)
            {
              dup2 (sortie, 1);
              close (sortie);
            }

          return monExec(cmd->argv [0], cmd->argv);
        }

      return pid;
}

// Gestion des pipes
int gestionPipes(int n, struct command *cmd){
      int i;
      pid_t pid;
      int fichier;
      int entree, fd [2];

      entree = 0;

      for (i = 0; i < n - 1; ++i)
        {
          pipe (fd);
          connectionPipe(entree, fd [1], cmd + i);
          close (fd [1]);
          entree = fd [0];
        }

      if (entree != 0)
        dup2 (entree, 0);

      printf("TEST\n\n");

      return monExec(cmd [i].argv [0], cmd [i].argv);
}


int estUnNombre(char* chaine){
    int i=0;
    int nombre = 1;
    while(i<strlen(chaine) && nombre){
        if(!isdigit(chaine[i]))
            nombre = 0;

        i++;
    }
    return nombre;
}

// fonction du TP noté
void cp(char* path1, char* path2){
    struct stat sts;

    stat(path1, &sts);

	if(S_ISDIR(sts.st_mode))
   		copieRep(path1, path2);
	else
		copie(path1, path2);

}

//etape 1
int copie(char *fic1, char *fic2){
    char buffer[BUFFER_SIZE];

    /*Initilisation des fichiers*/
    int source = open(fic1, O_RDONLY);
    int destination = open(fic2, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR);
    int check;

    /*on vÃ©rifie qu'il n'y a pas d'erreur Ã  l'ouverture*/
    if(destination == -1 || source == -1) return 0;

    /*on copie dans le fichier de destination*/
    int fin = 1;
    while(fin){
        check = read(source, buffer, BUFFER_SIZE);

        if(check!=0)
            write(destination, buffer, check);
        else{
            if(errno != EAGAIN)
                fin = 0;
            if(errno != EINTR)
                fin = 0;
        }
    }

    permissions(fic1, fic2);
    close(destination);
    close(source);

    return 1;
}

//etape 2
int permissions(char *fic1, char *fic2){
    struct stat sts;

    /*on obtien les donnÃ©es de fic1*/
    stat(fic1, &sts);

    /*on change les permissions de fic2*/
    if(!chmod(fic2, sts.st_mode)) return 1;
    else{
        printf("Changement de permissions a echouer.\n ");
        return 0;
    }
}

//etape 3
int copieRep(char *rep1, char *rep2){
	//opendir, closedir, readdir
	DIR* source = NULL;
	DIR* destination = NULL;
	struct dirent* dir;
	struct stat sts;
	int lenSource = strlen(rep1);
	int lenDest = strlen(rep2);
	int len = 0;
	char str_Source[BUFFER_SIZE], str_Dest[BUFFER_SIZE];

	//Ouverture du dossier
	if ((source = opendir(rep1)) == NULL)
	{
		printf("\nErreur d'ouverture source");
		return 1;
	}

	stat(rep1, &sts);

	mkdir(rep2, sts.st_mode);

	if ((destination = opendir(rep2)) == NULL)
	{
		printf("\nErreur d'ouverture destination");
		return 1;
	}

	while ((dir = readdir(source)) != NULL)
	{
		if (!(strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")))
            continue;

		len = strlen(dir->d_name);
		memcpy(str_Source, rep1, lenSource);
		if (rep1[lenSource] == '/')
		{
			memcpy(&(str_Source[lenSource]), dir->d_name, len);
			str_Source[lenSource + len] = '\0';
		}
		else
		{
			str_Source[lenSource] = '/';
			memcpy(&(str_Source[lenDest + 1]), dir->d_name, len);
			str_Source[lenSource + len + 1] = '\0';
		}

		memcpy(str_Dest, rep2, lenDest);
		if (rep2[lenDest] == '/')
		{
			memcpy(&(str_Dest[lenDest]), dir->d_name, len);
			str_Dest[lenDest + len] = '\0';
		}
		else
		{
			str_Dest[lenDest] = '/';
			memcpy(&(str_Dest[lenDest + 1]), dir->d_name, len);
			str_Dest[lenDest + len + 1] = '\0';
		}

		stat(str_Source, &sts);

		if(S_ISDIR(sts.st_mode))
   			copieRep(str_Source, str_Dest);
		else
			copie(str_Source, str_Dest);
		printf("Copie de %s\n", str_Source);
	}

	//Fermeture des dossiers
	closedir(source);
	closedir(destination);
}

// Fonction cat
void cat(char **args, int esp){
    int n=0;
    int j=1;
    for(int i = 1;i<esp;i++){
        if(!strcmp(args[i], "-n"))
            n = 1;
    }
    FILE* fichier = NULL;
    char chaine[10000] = "";

    for(int i = 1;i<esp;i++){
        if(strcmp(args[i], "-n")){
            fichier = fopen(args[i], "r");
            while (fgets(chaine, 10000, fichier) != NULL){
                if(n==1)
                    printf("%d %s", j, chaine);
                else
                    printf("%s", chaine);
                j++;
            }

            fclose(fichier);
        }

    }
}

// Fonction touch
void touch(char **args, int esp){
    //printf("%s\n", args[2]);
    if(args[1] == NULL){
        fprintf(stderr, "Veuillez renseigner un fichier\n");
        exit(1);
    }
    else if(args[2] == NULL){
        FILE* fichier = NULL;
        fichier = fopen(args[1], "w");

        fclose(fichier);
    }
    /*else if(!strcmp(args[2], "-g")){
        FILE* fichier = NULL;
        fichier = fopen(args[1], "w");

        struct utimbuf ubuf;
        ubuf.modtime = time(NULL);
        ubuf.actime = time(NULL);
        utime(args[1], &ubuf);

        fclose(fichier);
    }*/
    else{
        FILE* fichier = NULL;

        for(int i=1;i<esp;i++){
            if(strcmp(args[i], "-m")){
                if(i!=esp-1 && !strcmp(args[i+1], "-m")){
                    //printf("DLLD\n");
                    fichier = fopen(args[i], "a");

                    struct utimbuf ubuf;
                    ubuf.modtime = time(NULL);
                    ubuf.actime = time(NULL);
                    utime(args[i], &ubuf);

                    fclose(fichier);
                    //i++;
                }
                else{
                    FILE* fichier = NULL;
                    fichier = fopen(args[i], "w+");

                    fclose(fichier);
                }
            }
            /*else{
                FILE* fichier = NULL;
                fichier = fopen(args[1], "w+");

                fclose(fichier);
            }*/
        }
    }
}

// Fonction history
void hist(char **args){
    FILE* fichier = NULL;
    char chaine[10000] = "";

    fichier = fopen("historique.txt", "r");

    if (fichier != NULL)
    {
        if(args[1] == NULL){
            int i=1;
            while (fgets(chaine, 10000, fichier) != NULL){
                printf("%d %s", i, chaine);
                i++;
            }

        }
        else if(estUnNombre(args[1])){
            int nbL = 1;
            char car;
            do
            {
                car = fgetc(fichier);
                if(car == '\n')
                    nbL++;
            }while(car != EOF);
            int n = nbL - atoi(args[1]);
            int i=1;
            rewind(fichier);
            while (fgets(chaine, 10000, fichier) != NULL){

                if(i>=n){
                    printf("%d %s", i, chaine);
                }

                i++;
            }
        }
        else if(args[1][0] == '!'){
            char *nC = malloc((strlen(args[1])+1) * sizeof(char));
            char *commande;
            int esp;
            //char **args;
            //printf("%d\n", strlen(args[1]));
            for(int i = 1;i<strlen(args[1]);i++){
                nC[i-1] = args[1][i];
            }

            int n = atoi(nC);
            int i = 1;

            while (fgets(chaine, 10000, fichier) != NULL){

                if(i==n){

                    commande = malloc((strlen(chaine)+1) * sizeof(char));
                    strcpy(commande, chaine);
                }

                i++;
            }
            esp = compterEspace(commande)+1;

            char **argsCom;
            argsCom = malloc(esp * sizeof(char));
            for(int i=0;i<esp;i++){
                argsCom[i] = (char*)malloc(1000*sizeof(char));
            }

            char *entry = NULL;
            entry = strchr(commande, '\n');
            if(entry != NULL)
                *entry = '\0';

            couper(commande, argsCom);

            execvp(argsCom[0], argsCom);

            free(nC);
            for(int i=0;i<esp;i++){
                free(argsCom[i]);
            }
            free(argsCom);
            free(commande);

        }

        fclose(fichier);
    }

}

// Permet de remplir le fichier de gestion de l'historique
void remplirHist(char saisie[]){
    FILE* fichier = NULL;
    fichier = fopen("historique.txt", "a");
    if (fichier != NULL)
    {

        fputs(saisie, fichier);
        fputc('\n', fichier);

        fclose(fichier);
    }
}

// Permet l'allocation en mémoire du tableau d'arguments
int compterEspace(char saisie[100]){
    int c = 0;
    for(int i=0;i<strlen(saisie);i++){
        if(i<strlen(saisie)-1 && saisie[i]==' ' && saisie[i+1]!='|'){
            c++;
        }
        else if(i>=strlen(saisie)-1 && saisie[i]==' ')
            c++;
    }
    return c;
}

int compterPipe(char saisie[100]){
    int c = 0;
    for(int i=0;i<strlen(saisie);i++){
        if(saisie[i]=='|')
            c++;
    }
    return c;
}

void afficher(char **res, int nb){
    for(int i=0;i<nb;i++){
        printf("%s ", res[i]);
    }
    printf("\n");
}

// Permet de découper une commande saisie au bon format
void couper(char ch[], char **res){
    int i = 0;
    int nbMot = 0;
    int nbCar = 0;
    while(ch[i] == ' ') i++;


    while(i<strlen(ch)){
        if(ch[i] != ' '){
            res[nbMot][nbCar] = ch[i];
            nbCar++;
        }
        else{
            res[nbMot][nbCar] = '\0';
            nbMot++;
            nbCar = 0;
        }
        res[nbMot][nbCar] = '\0';

        i++;
    }
}



