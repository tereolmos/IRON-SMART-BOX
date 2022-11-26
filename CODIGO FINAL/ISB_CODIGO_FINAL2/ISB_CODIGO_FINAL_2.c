#include <16F877A.h>
#fuses XT, NOWDT 
#use delay (clock = 4 MHz) 
#use standard_io(B)
#use RS232 (BAUD=9660, BITS=8, PARITY=N, XMIT=PIN_C6, RCV=PIN_C7) //TX=C6, RX=C7

//LIBRERÍAS
#include <LCD_D.c> // LCD en el puerto D
#include <DS18B20.h> // Sensor de temperatura en B1

// ENTRADAS
#define INFRAROJO PIN_B0
#define FIN_E1_UP PIN_B3
#define FIN_E1_DOWN PIN_B2

//SALIDAS
#define E1_UP PIN_B6
#define E1_DOWN PIN_B7
#define RESISTENCIA PIN_C5
#define MOTOR_MEC PIN_A1
#define NIVELES_VAPOR PIN_A2

//AUXILIARES
#define ARRIBA 0
#define ABAJO 1
#define PARO 2
#define ON 1
#define OFF 0
#define CARGA 3036
#define TEM_DESEADA 50

// Variables globales para interrupción de tiempo
int1 un_segundo = 0;
int1 comenzar_conteo = 0;
int8 t_actual = 0;

// Variables globales para interrupción de recepción de datos
char start = '0', t1 = '0', t2 = '0', prendas = '0';

//Variables para enviar a LABVIEW
int8 temp_lv, n_pren_plan = 0, final = 0;

#INT_TIMER1
void interrupcion ()//que se detenga si start=0
{
   set_timer1(CARGA);
   un_segundo++;
   if (comenzar_conteo){t_actual++;}
}

#INT_RDA  //Interrupcion de dato recibido
void interrupcion_RDA () 
{
      start = getc();
      getc();
      t1 = getc();
      t2 = getc();
      getc();
      prendas = getc();
}

//PROTOTIPOS DE FUNCIONES
float medir_temperatura(void);
void control_temperatura (int8 *, int8 *);
void revisar_fines_carrera (int8 *);
void elevador_arriba (int8 *,int *);
void UPDOWN (int8, int8 *);
void control_pwm (int1, int8 *);
void informacion_enteros (int *, int8 *);
int conversion(char);
void detener_todo (int8 *, int8 *);
void delay_revisar_start (int, int8 *, int8 *);
void cambio_nivel ();
void enviar_labview ();
void basicos (int8 *, int8 *);
void control_plancha (int1);


void main ()
{
   //Variables
   int8 n_prendas_total = 0; // #Prendas colocadas por el usuario
   int8 n_prendas_actual = 0; // #Prendas ya planchadas
   int8 tiempo = 0; // Tiempo de planchado por prenda deseado
   int8 flag_motor_mec = 0; // Bandera estado del motor del mecanismo
   int8 flag_motor_elev = 0; // Bandera estado del motor del elevador
  
   
   // Imprimir título del proyecto=============================================
   lcd_init();
   lcd_putc('\f');
   lcd_gotoxy(1,1);
   printf(lcd_putc,"   IRON SMART");
   lcd_gotoxy(1,2);
   printf(lcd_putc,"       BOX      ");
   delay_ms(700);
   lcd_putc('\f');
   
   // Comenzar con los motores en PARO=========================================
   UPDOWN (PARO, &flag_motor_elev);
   lcd_putc('\f');
   
   //Set de las interrupciones (Timer 1 - 0.5s)================================
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8 );//Contador asíncrono con prescaler de 1
   enable_interrupts (INT_TIMER1);
   enable_interrupts (GLOBAL);
   set_timer1(CARGA);
   enable_interrupts(INT_RDA); //Habilitacion de la interrupcion de transmision
   
   //Comenzar con el elevador arriba===========================================
   elevador_arriba (&flag_motor_elev, &flag_motor_mec);
   
   //Mantener el pin de control de niveles en bajo
   output_low(NIVELES_VAPOR);
   
   
   while (TRUE)
   {
            
      if(start == '1') // Si se presionó el botón de inicio en LABVIEW
      {
         informacion_enteros (&tiempo, &n_prendas_total); //Convierte tiempo y numero de prendas deseadas a número entero
         control_plancha(ON); //Enciende la plancha
         control_temperatura (&flag_motor_mec, &flag_motor_elev); // Se realiza la medicion de temperatura cada segundo hasta llegar al set point (Espera mientras comienza a salir el vapor)
         while(n_prendas_actual < n_prendas_total)
         {
            lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
            if  (input(INFRAROJO)== 1)//No hay prenda detectada
            {
               lcd_gotoxy(1,2); printf(lcd_putc,"P:0");
               control_pwm (ON, &flag_motor_mec); //Se enciende el motor del mecanismo giratorio
               while (input(INFRAROJO)== 1) //Mientras no haya prenda detectada, solo se encarga de enviar datos a labview y revisar paro
               {
                  basicos(&flag_motor_mec, &flag_motor_elev);
               }
               lcd_gotoxy(1,2); printf(lcd_putc,"P:1");// el ciclo se rompe cuando detecta la pieza 
               control_pwm (OFF, &flag_motor_mec); //Se apaga el motor del mecanismo giratorio
               UPDOWN (ABAJO, &flag_motor_elev);//El elevador comienza a bajar
               comenzar_conteo = 1;
       
               while (t_actual/2 < tiempo) //UP & DOWN del elevador por el tiempo deseado
               {   
                  basicos(&flag_motor_mec, &flag_motor_elev); //enviar datos a labview y revisar paro
                  lcd_gotoxy(16,2); printf(lcd_putc,"%i ",(int)t_actual/2);
                  revisar_fines_carrera (&flag_motor_elev);
               }
               //UPDOWN (PARO, &flag_motor_elev); //Detiene el elevador
               elevador_arriba (&flag_motor_elev, &flag_motor_mec); // regresar cepillo arriba
               t_actual = 0;
               comenzar_conteo = 0;
               n_prendas_actual++;               
               lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
               n_pren_plan = n_prendas_actual;
               enviar_labview(); //Enviar datos a LABVIEW
               control_pwm (ON, &flag_motor_mec);
               delay_revisar_start (8, &flag_motor_elev, &flag_motor_mec); //La prenda avanza 2 seg para ya no ser detectada por el infrarrojo, revisando el start
               
               //elevador_arriba (&flag_motor_elev, &flag_motor_mec); // regresar cepillo arriba
               basicos(&flag_motor_mec, &flag_motor_elev); //enviar datos a labview y revisar paro

            }//if  (input(INFRAROJO)== 0) 
         }//while(n_prendas_actual < n_prendas_total)
         control_pwm (ON, &flag_motor_mec); //Se enciende el motor del mecanismo giratorio
         delay_revisar_start (8, &flag_motor_elev, &flag_motor_mec); //La prenda avanza 2 seg para ya no ser detectada por el infrarrojo, revisando el start
         control_pwm (OFF, &flag_motor_mec); //Se apaga el motor del mecanismo giratorio 
         control_plancha(OFF); //Apaga la plancha
         printf(lcd_putc,"\f   SUBIENDO...");
         elevador_arriba (&flag_motor_elev, &flag_motor_mec);// Creo q ya no es necesario
         printf(lcd_putc,"\f     PROCESO    \n    TERMINADO   ");//en espera de otro ciclo de planchado (se tiene que agregar un 1 nuevamente)==========
         final = 1;
         enviar_labview(); //Enviar datos a LABVIEW
         
         while(n_prendas_actual >= n_prendas_total)//agregar break si llega nuevo bit de inicio
         {
             while(start == '1')
             {
               
               if (un_segundo)
               {
                  temp_lv = medir_temperatura();
                  enviar_labview();
               }
               
             }
             //Se hace 0, se rompe el ciclo
             delay_ms(600);
             if (start == '1')//Regresa el 1
             {
               printf(lcd_putc,"\f     INICIO");
               delay_ms(750);
               printf(lcd_putc,"\f");
               final = 0;
               break;
             }
         }
         
         //===================================================NO QUITAR==================================================
         n_prendas_actual = 0;
         lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
         n_pren_plan = n_prendas_actual;
         enviar_labview(); //Enviar datos a LABVIEW
         //==============================================================================================================
         
      }//if(start == 1)

   } //while (TRUE)
} 

float medir_temperatura()
{
   signed int16 raw_temp;
   float temp;
   
   lcd_gotoxy(1,1);
   printf(lcd_putc,"T:");
   
   if (ds18b20_read(&raw_temp))
   {
      temp = (float)raw_temp/16;
      lcd_gotoxy(3,1);
      printf(lcd_putc,"%i", (int)temp);  
   }
   else
   {
      lcd_gotoxy(3,1);
      printf(lcd_putc,"E");
   }
   return temp;
}

void control_temperatura (int8 *flag_motor_mec, int8 *flag_motor_elev) 
{
   float temp = medir_temperatura(); 
   temp_lv = temp;
   enviar_labview(); //Enviar datos a LABVIEW
   while (temp <= TEM_DESEADA) //Cada segundo, mide e imprime la temperatura
   {
      if (un_segundo)
      {
         temp = medir_temperatura();
         temp_lv = temp;
         enviar_labview();
      }
      if (input(INFRAROJO)== 0 && *flag_motor_mec==1) //Si se detectó un objeto y está funcionando el motor del mecanismo
      {  
         break;      
      }
      if ((input(FIN_E1_UP) == 0 || input(FIN_E1_DOWN) == 0) && *flag_motor_elev==1) //Si el elevador llegó hasta arriba o hasta abajo y está funcionando el elevador***********
      {  
         break;      
      }
      if(start == '0')
      {
         detener_todo (&flag_motor_elev, &flag_motor_mec);
      }
   }

}

void control_plancha (int1 estado)
{
   if (estado == ON){output_low(RESISTENCIA); delay_ms(500); cambio_nivel ();lcd_gotoxy(14,1); printf(lcd_putc,"R:1");}
   else if (estado == OFF){output_low(RESISTENCIA);lcd_gotoxy(14,1); printf(lcd_putc,"R:0");}
   
}

void basicos (int8 *flag_motor_mec, int8 *flag_motor_elev) 
{
      if (un_segundo)
      {
         temp_lv = medir_temperatura();
         enviar_labview();
      }
      if(start == '0')
      {
         detener_todo (&flag_motor_elev, &flag_motor_mec);
      }
}

void revisar_fines_carrera (int8 *flag_motor_elev)
{
   if (input(FIN_E1_UP) == 0){UPDOWN (PARO, flag_motor_elev); delay_ms(500); UPDOWN (ABAJO, flag_motor_elev);lcd_gotoxy(5,2); printf(lcd_putc,"U:1");}
   else if (input(FIN_E1_UP) == 1){lcd_gotoxy(5,2); printf(lcd_putc,"U:0");}
   if (input(FIN_E1_DOWN) == 0){UPDOWN (PARO, flag_motor_elev); delay_ms(500); UPDOWN (ARRIBA, flag_motor_elev);lcd_gotoxy(9,2); printf(lcd_putc,"D:1");}
   else if (input(FIN_E1_DOWN) == 1){lcd_gotoxy(9,2); printf(lcd_putc,"D:0");}
}

void elevador_arriba (int8 *flag_motor_elev,int8 *flag_motor_mec)
{
   UPDOWN (ARRIBA, flag_motor_elev); //subir
   while (input(FIN_E1_UP) == 1) 
   {
      //if(start == '0'){UPDOWN (PARO, flag_motor_elev);}
      basicos(&flag_motor_mec, &flag_motor_elev); //enviar datos a labview y revisar paro
      if (start == '1'){UPDOWN (ARRIBA, flag_motor_elev);}
   }//ciclado mientras el elevador no esté hasta arriba
   UPDOWN (PARO, flag_motor_elev); delay_ms(100); 
}

void UPDOWN (int8 sentido, int8 *flag_motor_elev)
{
   if (sentido == ARRIBA){output_high(E1_UP); output_low(E1_DOWN); *flag_motor_elev = 1; lcd_gotoxy(10,1); printf(lcd_putc,"M:U");}
   else if (sentido == ABAJO) {output_high(E1_DOWN); output_low(E1_UP); *flag_motor_elev = 1; lcd_gotoxy(10,1); printf(lcd_putc,"M:D");}
   else if (sentido == PARO) {output_low(E1_DOWN); output_low(E1_UP); *flag_motor_elev = 0; lcd_gotoxy(10,1); printf(lcd_putc,"M:P");}
}

void control_pwm (int1 motor_flag, int8 *flag_motor_mec) 
{ 
   if (motor_flag)
   {
      output_high(MOTOR_MEC);
      lcd_gotoxy(6,1); printf(lcd_putc,"M:1");
      *flag_motor_mec = 1;
      
   }
   else
   {
      output_low(MOTOR_MEC);
      lcd_gotoxy(6,1); printf(lcd_putc,"M:0");
      *flag_motor_mec = 0;
   }
}

int conversion(char c)
{
   int num = 0;
   num = (int)c-'0';
   
   return num;
}

void informacion_enteros (int *tiempo, int8 *n_prendas_total)
{
   *tiempo = 10*conversion(t1)+conversion(t2);
   *n_prendas_total = conversion(prendas);
   lcd_gotoxy(1,1);
   printf(lcd_putc,"\fTIEMPO: %i", *tiempo);
   lcd_gotoxy(1,2);
   printf(lcd_putc,"PRENDAS: %i", *n_prendas_total);
   delay_ms(5000);
   printf(lcd_putc,"\f");
}

void detener_todo (int8 *flag_motor_elev, int8 *flag_motor_mec)
{
   int1 bandera_elevador_aux = 0;//Saber si el elevador estaba funcionando para detener el conteo de los segundos
   int1 bandera_mecanismo_aux = 0;//Saber si el mecanismo estaba funcionando
   
   if (*flag_motor_elev == 1){bandera_elevador_aux = 1;}
   if (*flag_motor_mec == 1){bandera_mecanismo_aux = 1;}
   control_plancha(OFF); //Apaga la plancha
   UPDOWN (PARO, flag_motor_elev); //Detener el elevador
   control_pwm (OFF, flag_motor_mec); //Detener motor de la banda
   comenzar_conteo = 0;//Detiene el conteo mientras start = 0
   
   while (start == '0')
   {
      
   }
   if (bandera_elevador_aux){comenzar_conteo = 1;} //Continúa con el conteo
   if (bandera_mecanismo_aux){control_pwm (ON, &flag_motor_mec);} //Encender nuevamente el mecanismo
   control_plancha(ON);//Enciende de nuevo la plancha 
   
}

void delay_revisar_start (int repeticiones, int8 *flag_motor_elev, int8 *flag_motor_mec)
{ 
   for (int x = 0; x<repeticiones; x++)
   {
      if(start == '0'){detener_todo (flag_motor_elev, flag_motor_mec);}
      delay_ms(250); //La prenda avanza un poquito para ya no ser detectada por el infrarojo
   }
}

void cambio_nivel ()
{
    int nivel = 0;
    
    if (nivel <= 4)
    {
          for (int i=0; i<=4; i++)
          {
             output_high(NIVELES_VAPOR);
             delay_ms(250);//está bien ese tiempo?
             output_low(NIVELES_VAPOR);
             nivel++;
             printf(lcd_putc,"\fNIVEL: %i", nivel);
          }
    }

}

void enviar_labview ()
{
   printf("%i;%i;%i\n", temp_lv, n_pren_plan, final);
}
