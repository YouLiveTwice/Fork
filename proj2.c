#include "proj2.h"

//deklarace funkci
void bus_f(int c,int atb);
int bus_logick();
void rid_f();
void clos_shm();
void clos_sem();

//vitvoreni semaphoru
sem_t *data, *rid_stop , *bus_stop, *rid_bus, *bus_out, *kill_rid;
//globalni promeny
typedef struct global_var{
	bool BUS_CICLE;
	int KILL_I;
	int END;
	int INS_I;		//por pocitani instrukci
	int RID_I;		//citadlo cestujicich
	int STOP_I;		//citadlo poctu cestujicich na zastavce
} global_var;
int ID;
global_var *central_info = NULL;
//
int R,C,ART,ABT;
int RID_ID;
//soubor
FILE *out;
//pomocna makra
#define SAFE_PRINT(STREAM,...)   do {fprintf(STREAM, __VA_ARGS__); fflush(STREAM);} while (0);

int main(int argc, char *argv[]){
	int R;
	int C;
	int ART;
	int ABT;
	if(argc == 5)
	{
		R = atoi(argv[1]);
		C = atoi(argv[2]);
		ART = atoi(argv[3]);
		ABT = atoi(argv[4]);
	}

	if ((argc == 5) && (ART >= 0 && ART <= 1000) && (ABT >= 0 && ABT <= 1000) && (R > 0) && (C > 0))
	{
	

		out = fopen("proj2.out", "w");
		if (out == NULL){
			fprintf(stderr, "nepodarylo se otevryt soubor \n");
			exit(1);
		}
		//vitvoreni zdilene pameti 
		ID = shm_open("/xhalmo00_shard", O_CREAT | O_EXCL | O_RDWR, 0644);
		if (ID < 0){ 
			fprintf(stderr , "Nebylo mozne vitvorit zdilenou pamet\n");
			exit(1);
		}
		ftruncate (ID, sizeof (global_var));
		central_info = mmap (NULL, sizeof (global_var), PROT_READ | PROT_WRITE, MAP_SHARED,ID,0);

		central_info -> BUS_CICLE = true;
		central_info -> KILL_I = 0;
		central_info -> END = R;
		central_info -> INS_I = 0;
		central_info -> RID_I = 1;
		central_info -> STOP_I = 0;
		//inicializace semaforu lock == 0  unloct > 0 
		data = sem_open("/xhalmo00_data", O_CREAT | O_EXCL, 0666, 1);
		rid_stop = sem_open("/xhalmo00_rid_stop", O_CREAT | O_EXCL, 0666,1);
		rid_bus = sem_open("/xhalmo00_rid_bus", O_CREAT | O_EXCL, 0666, 0);
		bus_stop = sem_open("/xhalmo00_bus_stop", O_CREAT | O_EXCL, 0666, 1);
		bus_out = sem_open("/xhalmo00_bus_out", O_CREAT | O_EXCL, 0666, 0);
		kill_rid = sem_open("/xhalmo00_kill_rid", O_CREAT | O_EXCL, 0666, 0);
  
		
		
		srand(time(NULL));
		// vitvoreni procesu pro bus
		int bus = fork();
		int id_bus = getpid();
		if (bus == 0)
		{
			bus_f(C,ABT);
		}
		else
		{
			// proces pro generovani ridru
			int rid = fork();
			int id_rid = getpid();
			if (rid == 0)
			{
				// ciklus  pro generovani ridru
				int rid_c;
				for(int i = 0; i < R; i++)
				{
					//nemusi byt return vsechny procesi se ukonci jiz v finkci rid_f
					rid_c = fork();
					if(rid_c == 0)
					{
						rid_f();
					}
					else 
					{
						//pauza mezi gnenerovani procese z argumentu
						if(ART > 0)
						{
							int random_num = rand() % ART + 0;
							usleep(random_num);
						}
					}
				} 
			}
			//cekani na ukonceni procesu ktery gnenruje rid
			waitpid(id_rid,NULL,0);
			exit(0);
		}
		//cekani na ukonceni procesu bus
		waitpid(id_bus,NULL, 0);
	}
	//pri spatnych zadani argumentu
	else
	{
		fprintf(stderr,"spatne zadane argumenty\n");
		exit(1);
	}
	clos_shm();
	clos_sem();
	return 0;
}

// funkce pro bus
void bus_f(int c,int abt){
	sem_wait(data);
	central_info -> INS_I ++;
	SAFE_PRINT(out, "%i\t: BUS\t: start\n", central_info -> INS_I);
	sem_post(data);
	//BUS_CICLE se zmeni na false az poslednim procesem rid
	while (central_info -> BUS_CICLE)
	{
		//jen pokud je nekdo na zastavce
		if (central_info->STOP_I > 0)
		{
			//zamcit aby se nemohly dostat ridr na zatavku
			sem_wait(rid_stop);

			sem_wait(data);
			central_info -> INS_I++;
			SAFE_PRINT(out, "%i\t: BUS\t: arrival\n",central_info -> INS_I);
			sem_post(data);

			//procedese jen kdyz je vice poctu volnich mist v buse
			if (central_info ->STOP_I < c)
			{
				sem_wait(data);
				central_info -> INS_I ++;
				central_info -> KILL_I = central_info -> STOP_I;
				SAFE_PRINT(out,"%i\t: BUS\t: start boarding: %i\n",central_info->INS_I,central_info->STOP_I);
				sem_post(data);

				for(int i = 0; i < central_info->STOP_I; i++){
					sem_post(rid_bus);
				}
				//zamcit proces bus a rid je pak odemce 
				for(int i = 0; i < central_info->STOP_I; i++ ){	
					sem_wait(bus_out);
				}
			}
			//provedese jen kdyz je na zatavce vice lidi jak je volnich mist
			else
			{
				sem_wait(data);
				central_info -> INS_I ++;
				central_info -> KILL_I = c;
				SAFE_PRINT(out,"%i\t: BUS\t: start boarding: %i\n",central_info->INS_I,c);
				sem_post(data);
	
				for(int i = 0; i < c; i++){
					sem_post(rid_bus);
				}
				//zamict proces bus a rid je pak odemce
				for(int i = 0; i < c; i++ ){	
					sem_wait(bus_out);
				}

			}
		
			sem_wait(data);
			central_info -> STOP_I = central_info->STOP_I - central_info->KILL_I;
			central_info -> INS_I ++;
			SAFE_PRINT(out,"%i\t: BUS\t: end boarding: %i\n", central_info->INS_I, central_info->STOP_I);		
			sem_post(data);
		
			sem_wait(data);
			central_info -> INS_I ++;
			SAFE_PRINT(out,"%i\t: BUS\t: depart\n", central_info->INS_I);
			sem_post(data);

			//zamcit proces rid aby se nemohly dostat do autobusu
			sem_wait(data);
			sem_post(rid_bus);
			sem_wait(rid_bus);
			sem_post(data);

			//odemcit rid aby se mohly dostat na zastavku
			sem_post(rid_stop);

			if(abt > 0)
			{
				int random_num = rand() % abt + 0;
				usleep(random_num);
			}
			
			sem_wait(data);
			central_info -> INS_I++;
			SAFE_PRINT(out,"%i\t: BUS\t: end\n", central_info->INS_I);
			sem_post(data);
			
			//odemceni rid aby se mohly ukoncit
			sem_wait(data);
			for (int i = 0; i < central_info->KILL_I; i++)
			{
				sem_post(kill_rid);
			}
			sem_post(data);
		}
		// provedese jen kdy nikdo neni na zastavce
		else
		{

			sem_wait(data);
			central_info -> INS_I++;
			SAFE_PRINT(out, "%i\t: BUS\t: arrival\n",central_info -> INS_I);
			sem_post(data);

			sem_wait(data);
			central_info -> INS_I ++;
			SAFE_PRINT(out,"%i\t: BUS\t: depart\n", central_info->INS_I);
			sem_post(data);
			
			if(abt > 0){
				int random_num = rand() % abt + 0;
				usleep(random_num);
			}	
		
			sem_wait(data);
			central_info -> INS_I ++;
			SAFE_PRINT(out,"%i\t: BUS\t: end\n", central_info->INS_I);
			sem_post(data);

		}
		
	}
	// ukonceni bus
	sem_wait(data);
	central_info -> INS_I ++;
	SAFE_PRINT(out,"%i\t: BUS\t: finish\n", central_info->INS_I);
	sem_post(data);
}

//funkce pro rid
void rid_f(){
	sem_wait(data);
	central_info -> INS_I ++;
	RID_ID=central_info -> RID_I ++;
	SAFE_PRINT(out, "%i\t: RID %i\t: start\n",central_info ->  INS_I,RID_ID );
	sem_post(data);
		
	//prichod na zastavku
	sem_wait(rid_stop);

	sem_wait(data);
	central_info -> INS_I ++;
	central_info -> STOP_I ++;
	SAFE_PRINT(out,"%i\t: RID %i\t: enter: %i\n",central_info->INS_I,RID_ID,central_info -> STOP_I);
	sem_post(data);

	sem_post(rid_stop);
	//cekani na prijezd busu
	sem_wait(rid_bus);

	sem_wait(data);
	central_info -> INS_I ++;
	SAFE_PRINT(out,"%i\t: RID %i\t: boarding\n",central_info->INS_I,RID_ID);
	sem_post(data);
		
	//ukonceni nastupovani
	sem_post(bus_out);
	//cekani na povoleni se ukoncit
	sem_wait(kill_rid);

	
	sem_wait(data);
	central_info -> END --;
	central_info -> INS_I ++;
	SAFE_PRINT(out,"%i\t: RID %i\t: finish\n",central_info->INS_I,RID_ID);
	sem_post(data);
		
	//posledni proces rid zmeni tuto hodnotu	
	if (central_info -> END == 0)
	{	
		central_info->BUS_CICLE = false;
	}

	exit(0);
}

//uvolneni zdilene pametu
void clos_shm(){
	close(ID);
	munmap(central_info, sizeof (global_var));
	shm_unlink("/xhalmo00_shard");
	shmctl(ID,IPC_RMID, NULL);
}
//uvolneni semaforu
void clos_sem(){
	sem_unlink("/xhalmo00_data");
	sem_unlink("/xhalmo00_rid_stop");
	sem_unlink("/xhalmo00_rid_bus");
	sem_unlink("/xhalmo00_bus_stop");
	sem_unlink("/xhalmo00_bus_out");
	sem_unlink("/xhalmo00_kill_rid");

	sem_close(data);
	sem_close(rid_stop);
	sem_close(rid_bus);
	sem_close(bus_stop);
	sem_close(bus_out);
	sem_close(kill_rid);
}
