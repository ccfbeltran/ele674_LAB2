/*
 * Sensor.c
 *
 *  Created on: 12 sept. 2013
 *      Author: bruno
 */

#include "Sensor.h"


#define ABS(x) (((x) < 0.0) ? -(x) : (x))


#define MAX_TOT_SAMPLE 1000

pthread_barrier_t   SensorStartBarrier;
pthread_barrier_t   LogStartBarrier;
pthread_mutex_t 	Log_Mutex;

uint8_t  SensorsActivated 	= 0;
uint8_t  LogActivated  	  	= 0;
uint8_t  numLogOutput 	  	= 0;

void *SensorTask ( void *ptr ) {
/* A faire! */
/* Tache qui sera instancié pour chaque sensor. Elle s'occupe d'aller */
/* chercher les donnees du sensor.                                    */
	uint32_t retval;
	SensorRawData tampon_raw_data;
	uint64_t time_stamp_avant=0;

	pthread_barrier_wait(&(SensorStartBarrier));
	/*on transforme notre pointeur vide en pointer SensorRawData*/
	SensorStruct *Sensor = (SensorStruct *)ptr;

	while (SensorsActivated) {




		/*lire la donne du capteur et le garder dans sonr_raw_data*/

		read(Sensor->File,&tampon_raw_data,sizeof(SensorRawData));



		pthread_mutex_lock(&(Sensor->DataSampleMutex));
		Sensor->DataIdx++;
		Sensor->DataIdx=Sensor->DataIdx % DATABUFSIZE;
		pthread_spin_lock(&(Sensor->DataLock));
		Sensor->RawData[Sensor->DataIdx]=tampon_raw_data;

		if(Sensor->type == SONAR || Sensor->type == BAROMETRE){
			Sensor->Data[Sensor->DataIdx].Data[0] = Sensor->RawData[Sensor->DataIdx].data[0]*Sensor->Param->Conversion;
		}
		else{
			Sensor->Data[Sensor->DataIdx].Data[0] = Sensor->RawData[Sensor->DataIdx].data[0]*Sensor->Param->Conversion;
			Sensor->Data[Sensor->DataIdx].Data[1] = Sensor->RawData[Sensor->DataIdx].data[1]*Sensor->Param->Conversion;
			Sensor->Data[Sensor->DataIdx].Data[2] = Sensor->RawData[Sensor->DataIdx].data[2]*Sensor->Param->Conversion;
		}
		/*on ajuste le time stamp*/
		Sensor->Data[Sensor->DataIdx].TimeDelay= tampon_raw_data.timestamp-time_stamp_avant;
		/*on sauvegarde la valeur precedante*/
		time_stamp_avant=tampon_raw_data.timestamp;



		pthread_spin_unlock(&(Sensor->DataLock));

		pthread_cond_broadcast(&(Sensor->DataNewSampleCondVar));
		pthread_mutex_unlock(&(Sensor->DataSampleMutex));

		/*on envoie la donne dans le log pour pouvoir l'afficher*/
//		InitSensorLog(Sensor->RawData);
//		if ((retval = InitSensorLog(Sensor->RawData)) < 0) {
//			printf("%s : Impossible d'initialiser log pour rawdata. => retval = %d\n", __FUNCTION__, retval);
//		}

//		DOSOMETHING();
	}
	pthread_exit(0); /* exit thread */
}


int SensorsInit (SensorStruct SensorTab[NUM_SENSOR]) {
/* A faire! */
/* Ici, vous devriez faire l'initialisation de chacun des capteurs.  */ 
/* C'est-à-dire de faire les initialisations requises, telles que    */
/* ouvrir les fichiers des capteurs, et de créer les Tâches qui vont */
/* s'occuper de réceptionner les échantillons des capteurs.          */
//

	// { ACCEL_INIT, GYRO_INIT, MAGNETO_INIT, BAROM_INIT, SONAR_INIT };

	int retval = 0;
	pthread_barrier_init(&SensorStartBarrier, NULL, NUM_SENSOR + 1);
	for(int i =0; i < NUM_SENSOR; i++)
	{
		pthread_mutex_init(&(SensorTab[i].DataSampleMutex), NULL);
		pthread_spin_init(&(SensorTab[i].DataLock), PTHREAD_PROCESS_PRIVATE);
		pthread_cond_init(&(SensorTab[i].DataNewSampleCondVar), NULL);

		SensorTab[i].File = open(SensorTab[i].DevName, O_RDONLY);

		if(SensorTab[i].File < 0){
			printf("Open capteur %d and DevName %s: Impossible douvrir\n", i, SensorTab[i].DevName);
			return -1;
		}
		printf("Open capteur %d succes\n", i);
		retval = pthread_create(&(SensorTab[i].SensorThread), NULL, SensorTask, (void*) &(SensorTab[i]));

		if(retval){
			printf("pthread_create : Impossible le thread %d\n", i);
			return retval;
		}
	}

	printf("SensorsInit succes\n");

	return retval;
};


int SensorsStart (void) {
/* A faire! */
/* Ici, vous devriez démarrer l'acquisition sur les capteurs.        */ 
/* Les capteurs ainsi que tout le reste du système devrait être      */
/* prêt à faire leur travail et il ne reste plus qu'à tout démarrer. */

	SensorsActivated = 1;
	pthread_barrier_wait(&(SensorStartBarrier));
	pthread_barrier_destroy(&(SensorStartBarrier));
	printf("%s Sensors démarré\n", __FUNCTION__);

	return 0;
}


int SensorsStop (SensorStruct SensorTab[NUM_SENSOR]) {
/* A faire! */
/* Ici, vous devriez défaire ce que vous avez fait comme travail dans */
/* SensorsInit() (toujours verifier les retours de chaque call)...    */ 

	int err = 0;

	SensorsActivated = 0;

	for(int i = 0; i < NUM_SENSOR; i++){
		err	= pthread_join(SensorTab[i].SensorThread, NULL);

		if(err){
			printf("pthread_join(SensorThread[%d]) : Erreur\n", i);
			return err;
		}
		pthread_mutex_destroy(&(SensorTab[i].DataSampleMutex));
		pthread_spin_destroy(&(SensorTab[i].DataLock));
		pthread_cond_destroy(&(SensorTab[i].DataNewSampleCondVar));
	}

	return err;
}



/* Le code ci-dessous est un CADEAU !!!	*/
/* Ce code permet d'afficher dans la console les valeurs reçues des capteurs.               */
/* Évidemment, celà suppose que les acquisitions sur les capteurs fonctionnent correctement. */
/* Donc, dans un premier temps, ce code peut vous servir d'exemple ou de source d'idées.     */
/* Et dans un deuxième temps, peut vous servir pour valider ou vérifier vos acquisitions.    */
/*                                                                                           */
/* NOTE : Ce code suppose que les échantillons des capteurs sont placés dans un tampon       */
/*        circulaire de taille DATABUFSIZE, tant pour les données brutes (RawData) que       */
/*        les données converties (NavData) (voir ci-dessous)                                 */

void *SensorLogTask ( void *ptr ) {
	SensorStruct	*Sensor    = (SensorStruct *) ptr;
	uint16_t		*Idx       = &(Sensor->DataIdx);
	uint16_t		LocalIdx   = DATABUFSIZE;
	SensorData	 	*NavData   = NULL;
	SensorRawData	*RawData   = NULL;
	SensorRawData   tpRaw;
	SensorData 	    tpNav;
	double			norm;

	printf("%s : Log de %s prêt à démarrer\n", __FUNCTION__, Sensor->Name);
	pthread_barrier_wait(&(LogStartBarrier));

	while (LogActivated) {
		pthread_mutex_lock(&(Sensor->DataSampleMutex));
		while ((LocalIdx == *Idx)&&(LogActivated != 0))
			pthread_cond_wait(&(Sensor->DataNewSampleCondVar), &(Sensor->DataSampleMutex));
		LocalIdx = *Idx;
	    pthread_mutex_unlock(&(Sensor->DataSampleMutex));
		if (LogActivated == 0) {
		    pthread_mutex_unlock(&(Sensor->DataSampleMutex));
			break;
		}

	   	pthread_spin_lock(&(Sensor->DataLock));
    	NavData   = &(Sensor->Data[LocalIdx]);
    	RawData   = &(Sensor->RawData[LocalIdx]);
		memcpy((void *) &tpRaw, (void *) RawData, sizeof(SensorRawData));
		memcpy((void *) &tpNav, (void *) NavData, sizeof(SensorData));
	   	pthread_spin_unlock(&(Sensor->DataLock));

	   	pthread_mutex_lock(&Log_Mutex);
		if (numLogOutput == 0)
			printf("Sensor  :     TimeStamp  SampleDelay  Status  SampleNum   Raw Sample Data  =>        Converted Sample Data               Norme\n");
		else switch (tpRaw.type) {
				case ACCELEROMETRE :	norm = sqrt(tpNav.Data[0]*tpNav.Data[0]+tpNav.Data[1]*tpNav.Data[1]+tpNav.Data[2]*tpNav.Data[2]);
										printf("Accel   : (%09llu)-(%09u)  %2d     %8u   %04X  %04X  %04X  =>  %10.5lf  %10.5lf  %10.5lf  =  %10.5lf\n", tpRaw.timestamp, tpNav.TimeDelay, tpRaw.status, tpRaw.ech_num, (uint16_t) tpRaw.data[0], (uint16_t) tpRaw.data[1], (uint16_t) tpRaw.data[2], tpNav.Data[0], tpNav.Data[1], tpNav.Data[2], norm);
										break;
				case GYROSCOPE :		norm = sqrt(tpNav.Data[0]*tpNav.Data[0]+tpNav.Data[1]*tpNav.Data[1]+tpNav.Data[2]*tpNav.Data[2]);
										printf("Gyro    : (%09llu)-(%09u)  %2d     %8u   %04X  %04X  %04X  =>  %10.5lf  %10.5lf  %10.5lf  =  %10.5lf\n", tpRaw.timestamp, tpNav.TimeDelay, tpRaw.status, tpRaw.ech_num, (uint16_t) tpRaw.data[0], (uint16_t) tpRaw.data[1], (uint16_t) tpRaw.data[2], tpNav.Data[0], tpNav.Data[1], tpNav.Data[2], norm);
										break;
				case SONAR :			printf("Sonar   : (%09llu)-(%09u)  %2d     %8u   %04X              =>  %10.5lf\n", tpRaw.timestamp, tpNav.TimeDelay, tpRaw.status, tpRaw.ech_num, (uint16_t) tpRaw.data[0], tpNav.Data[0]);
										break;
				case BAROMETRE :		printf("Barom   : (%09llu)-(%09u)  %2d     %8u   %04X              =>  %10.5lf\n", tpRaw.timestamp, tpNav.TimeDelay, tpRaw.status, tpRaw.ech_num, (uint16_t) tpRaw.data[0], tpNav.Data[0]);
										break;
				case MAGNETOMETRE :		norm = sqrt(tpNav.Data[0]*tpNav.Data[0]+tpNav.Data[1]*tpNav.Data[1]+tpNav.Data[2]*tpNav.Data[2]);
										printf("Magneto : (%09llu)-(%09u)  %2d     %8u   %04X  %04X  %04X  =>  %10.5lf  %10.5lf  %10.5lf  =  %10.5lf\n", tpRaw.timestamp, tpNav.TimeDelay, tpRaw.status, tpRaw.ech_num, (uint16_t) tpRaw.data[0], (uint16_t) tpRaw.data[1], (uint16_t) tpRaw.data[2], tpNav.Data[0], tpNav.Data[1], tpNav.Data[2], norm);
										break;
			 }
		numLogOutput++;
		if (numLogOutput > 20)
			numLogOutput = 0;
		pthread_mutex_unlock(&Log_Mutex);
	}

	printf("%s : %s Terminé\n", __FUNCTION__, Sensor->Name);

	pthread_exit(0); /* exit thread */
}


int InitSensorLog (SensorStruct *Sensor) {
	pthread_attr_t		attr;
	struct sched_param	param;
	int					retval;

	pthread_attr_init(&attr);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
	pthread_attr_setschedpolicy(&attr, POLICY);
	param.sched_priority = sched_get_priority_min(POLICY);
	pthread_attr_setstacksize(&attr, THREADSTACK);
	pthread_attr_setschedparam(&attr, &param);

	printf("Creating Log thread : %s\n", Sensor->Name);
	if ((retval = pthread_create(&(Sensor->LogThread), &attr, SensorLogTask, (void *) Sensor)) != 0)
		printf("%s : Impossible de créer Tâche Log de %s => retval = %d\n", __FUNCTION__, Sensor->Name, retval);


	pthread_attr_destroy(&attr);

	return 0;
}


int SensorsLogsInit (SensorStruct SensorTab[]) {
	int16_t	  i, numLog = 0;
	int16_t	  retval = 0;

	for (i = 0; i < NUM_SENSOR; i++) {
		if (SensorTab[i].DoLog == 1) {
			numLog++;
		}
	}
	pthread_barrier_init(&LogStartBarrier, NULL, numLog+1);
	pthread_mutex_init(&Log_Mutex, NULL);

	for (i = 0; i < NUM_SENSOR; i++) {
		if (SensorTab[i].DoLog == 1) {
			printf("SensorTab[%d] =  %s\n", i, SensorTab[i].DevName);
			if ((retval = InitSensorLog(&SensorTab[i])) < 0) {
				printf("%s : Impossible d'initialiser log de %s => retval = %d\n", __FUNCTION__, SensorTab[i].Name, retval);
				return -1;
			}
		}
	}
	return 0;
};


int SensorsLogsStart (void) {
	LogActivated = 1;
	pthread_barrier_wait(&(LogStartBarrier));
	pthread_barrier_destroy(&LogStartBarrier);
	printf("%s NavLog démarré\n", __FUNCTION__);

	return 0;
};


int SensorsLogsStop (SensorStruct SensorTab[]) {
	int16_t	i;

	LogActivated = 0;
	for (i = 0; i < NUM_SENSOR; i++) {
		if (SensorTab[i].DoLog == 1) {
			pthread_join(SensorTab[i].LogThread, NULL);
			SensorTab[i].DoLog = 0;
		}
	}
	pthread_mutex_destroy(&Log_Mutex);

	return 0;
};


