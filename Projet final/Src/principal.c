#include "gassp72.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "etat.h"

#define pwn_periode 360 // KhZ
#define SYSTICK_PER 360000 // (360000 ticks �quivaut � 5ms)
#define M2TIR 984064 

type_etat etat;
extern void callback_son(void);
extern short Son;
extern int LongueurSon;
extern int PeriodeSonMicroSec;
extern int dft(short* signal, int k);
extern int TabSig[];

/*
CALCUL DE M2TIR
A/2 * 64 = 1982
puis 1982^2 =3936256
format : 16.0 * 1.15 => 17.15 ^ 2 => 34.30
Troncature des 32 bits les plus forts => 2 bits de la partie enti�re supprim�s
=> division par 4 donc 3936256/4 = 984064 = M2TIR
*/


// CONSTANTES -----------------------
short int DMA[64];
//Valeurs de k correspondant aux fr�quences des pistolets
int val_k[] = {17 , 18 ,19 ,20 ,23 ,24};
int compteur[] = {0,0,0,0,0,0};
int score[] = {0,0,0,0,0,0};

////////////////////////////////////////////////////////////////////////
////////////IMPORTANT : param�trage de la trame de signaux//////////////
////////////////////////////////////////////////////////////////////////

//D�commenter la trame � analyser :

int trame = 0x33; //Scores th�oriques : 1, 2, 3, 4, 5, 0
//int trame = 0x52; //Scores th�oriques : 1, 2, 3, 4, 5, 15 (0x0000000E)

////////////////////////////////////////////////////////////////////////
////////////IMPORTANT : param�trage de la trame de signaux//////////////
////////////////////////////////////////////////////////////////////////


// --------------------

void active_son() {
	
	GPIO_Configure(GPIOB, 0, OUTPUT, ALT_PPULL);
	// config TIM3-CH3 en mode PWM
	etat.resolution = PWM_Init_ff( TIM3, 3, pwn_periode );
	etat.son = &Son;
	etat.taille = LongueurSon;
	etat.position = 0;
	// 72000 tick = 1ms donc X periode us = 72tick*Y tick
	etat.periode_ticks = PeriodeSonMicroSec*72;
	
	// activation de la PLL qui multiplie la fr�quence du quartz par 9
	CLOCK_Configure();
	//config port PB1 pour �tre utilis� en sortie
	GPIO_Configure(GPIOB, 1, OUTPUT, OUTPUT_PPULL);
	// initialisation du timer 4
	// Periode_en_Tck doit fournir la dur�e entre interruptions,
	// exprim�e en p�riodes Tck de l'horloge principale du STM32 (72 MHz)
	Timer_1234_Init_ff( TIM4, etat.periode_ticks );
	// enregistrement de la fonction de traitement de l'interruption timer
	// ici le 2 est la priorit�, timer_callback est l'adresse de cette fonction, a cr��r en asm,
	// cette fonction doit �tre conforme � l'AAPCS
	Active_IT_Debordement_Timer( TIM4, 2, callback_son );
	// lancement du timer
	Run_Timer( TIM4 );
	
}
void sys_callback(void) {
	
	//On r�cup�re 64 valeurs du signal
	Start_DMA1(64);
	Wait_On_End_Of_DMA1();
	Stop_DMA1;
	
	//On it�re sur les 6 valeurs de "fr�quences" k des pistolets
	for (int i=0; i<6 ; i++) {
			if (dft(DMA, val_k[i]) > M2TIR) {
					compteur[i]++;
			}
			else {
					compteur[i] = 0;
			}
	}
	for (int i=0 ; i<6 ; i++ ) {
			if (compteur[i] == 13){
				score[i] +=1;
				//active_son();
			}
		}
}
int main(void)
{
	// activation de la PLL qui multiplie la fr�quence du quartz par 9
	CLOCK_Configure();
	// PA2 (ADC voie 2) = entr�e analog
	GPIO_Configure(GPIOA, 2, INPUT, ANALOG);
	// PB1 = sortie pour profilage � l'oscillo
	GPIO_Configure(GPIOB, 1, OUTPUT, OUTPUT_PPULL);
	// PB14 = sortie pour LED
	GPIO_Configure(GPIOB, 14, OUTPUT, OUTPUT_PPULL);

	// activation ADC, sampling time 1us
	//Le deuxi�me argument de cette fonction nous permet de d�terminer quelle trame de signaux
	//va �tre envoy�e.
	
	//Configuration de la trame de signaux
	/*
	
	Digit de      amplitude    amplitude    (en unites ADC)
faible poids     signal 1      bruit
     0 						 31 					 4
     1 						 62 					 4
     2 						 124 					 4
     3 					   248 					 4
     4 						 31 					 12
     5 						 62 					 12
     6 					   124 					 12
     7 						 248 					 12
     8 						 31 					 36
     9 						 62 					 36
     A 						 124 					 36
     B 						 248 					 36
     C 						 31 					 100
     D 						 62 					 100
     E 						 124 					 100
     F 						 248 					 100
		 
	Digit de      amplitude
 fort poids      signal 2
	   3 						 0
	   4 						 248
	   5 						 1000
		 
	*/
	
	Init_TimingADC_ActiveADC_ff( ADC1, trame );
	Single_Channel_ADC( ADC1, 2 );
	// D�clenchement ADC par timer2, periode (72MHz/320kHz)ticks
	Init_Conversion_On_Trig_Timer_ff( ADC1, TIM2_CC2, 225 );
	// Config DMA pour utilisation du buffer dma_buf (a cr��r)
	Init_ADC1_DMA1( 0, (vu16*)DMA );
	// Config Timer, p�riode exprim�e en p�riodes horloge CPU (72 MHz)
	Systick_Period_ff(SYSTICK_PER);
	// enregistrement de la fonction de traitement de l'interruption timer
	// ici le 3 est la priorit�, sys_callback est l'adresse de cette fonction, a cr��r en C
	Systick_Prio_IT( 3, sys_callback );
	SysTick_On;
	SysTick_Enable_IT;

	while	(1) {}
}
