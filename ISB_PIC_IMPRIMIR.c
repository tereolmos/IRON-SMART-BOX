#include <16F877A.h>
#fuses XT, NOWDT 
#use delay (clock = 4 MHz) 
#use standard_io(B)//REVISAR ESTO, SE OCUPA AÑADIR DE LOS DEMAS PUERTOS?
#use RS232 (BAUD=9660, BITS=8, PARITY=N, XMIT=PIN_C6, RCV=PIN_C7) //TX=C6, RX=C7

//LIBRERÍAS
#include <LCD_D.c> // LCD en el puerto D
#include <DS18B20.h> // Sensor de temperatura en B1

// ENTRADAS
#define INFRAROJO PIN_B0
#define FIN_E1_UP PIN_B2
#define FIN_E1_DOWN PIN_B3

//SALIDAS
#define E1_UP PIN_B6
#define E1_DOWN PIN_B7
#define RESISTENCIA PIN_C5

//AUXILIARES
#define ARRIBA 0
#define ABAJO 1
#define PARO 2
#define ON 1
#define OFF 0
#define CARGA 3036
#define TEM_DESEADA 29

int1 un_segundo = 0;
int1 comenzar_conteo = 0;
int8 t_actual = 0;

#INT_TIMER1
void interrupcion ()
{
   set_timer1(CARGA);
   un_segundo++;
   if (comenzar_conteo){t_actual++;}
}

//PROTOTIPOS DE FUNCIONES
float medir_temperatura(void);
void control_temperatura (int8 *, int8 *);
void revisar_fines_carrera (int8 *);
void UPDOWN (int8, int8 *);
void control_pwm (int1, int8 *);
void lectura_serial (int8 *, int8 *, int8 *, int8 *);



void main ()
{
   //Variables
   int8 start = 0; //Viene de LABVIEW, boton de INICIO
   int8 stop = 0; //Viene de LABVIEW, boton PARO DE EMERGENCIA
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
   
   //Set del módulo CCP para pwm del motor del mecanismo ======================
   setup_timer_2(t2_div_by_4,249,1);
   setup_ccp1(ccp_pwm);
   set_pwm1_duty(0);
   
   //Set de las interrupciones (Timer 1 - 0.5s)================================
   setup_timer_1(T1_INTERNAL|T1_DIV_BY_8 ); //Contador asíncrono con prescaler de 1
   enable_interrupts (INT_TIMER1);
   enable_interrupts (GLOBAL);
   set_timer1(CARGA);
   
   
   while (TRUE)
   {
      lectura_serial (&start, &stop, &tiempo, &n_prendas_total);//Realizar la lectura desde labview
      if(start == 1) // Si se presionó el botón de inicio
      {
         control_temperatura (&flag_motor_mec, &flag_motor_elev); // Se realiza la medicion de temperatura cada segundo hasta llegar al set point 
         while(n_prendas_actual < n_prendas_total)
         {
            lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
            if  (input(INFRAROJO)== 1)//No hay prenda detectada
            {
               lcd_gotoxy(1,2); printf(lcd_putc,"P:0");
               control_pwm (ON, &flag_motor_mec); //Se enciende el motor del mecanismo giratorio
               while (input(INFRAROJO)== 1) //Mientras no haya prenda detectada, solo se encarga de mantener la temperatura
               {
                  control_temperatura (&flag_motor_mec, &flag_motor_elev);
               }
               lcd_gotoxy(1,2); printf(lcd_putc,"P:1");// el ciclo se rompe cuando detecta la pieza 
               control_pwm (OFF, &flag_motor_mec); //Se apaga el motor del mecanismo giratorio
               UPDOWN (ARRIBA, &flag_motor_elev);
               comenzar_conteo = 1;
       
               while (t_actual/2 < tiempo)
               {
                  lcd_gotoxy(16,2); printf(lcd_putc,"%i ",(int)t_actual/2);
                  revisar_fines_carrera (&flag_motor_elev);
                  control_temperatura (&flag_motor_mec, &flag_motor_elev);
                  //agregar mantenimiento de temperatura?
               }
               UPDOWN (PARO, &flag_motor_elev);
               t_actual = 0;
               comenzar_conteo = 0;
               n_prendas_actual++;
               lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
               delay_ms(1000); //La prenda avanza un poquito para ya no ser detectada por el infrarojo==================================================
               control_temperatura (&flag_motor_mec, &flag_motor_elev);
               //control_pwm (ON, &flag_motor_mec);//ES NECESARIO????????
            }//if  (input(INFRAROJO)== 0) 
            
            
         }//while(n_prendas_actual < n_prendas_total)
         
         control_pwm (OFF, &flag_motor_mec); //Se apaga el motor del mecanismo giratorio
         //Agregar que regresen a la posicion inicial=============================================================================================
         UPDOWN (ABAJO, &flag_motor_elev);
         printf(lcd_putc,"\f   BAJANDO...");
         while (input(FIN_E1_DOWN) == 1);//mientras el elevador no haya llegado hasta abajo
         
         UPDOWN (PARO, &flag_motor_elev);
         printf(lcd_putc,"\f     PROCESO    \n    TERMINADO   ");//en espera de otro ciclo de planchado (se tiene que agregar un 1 nuevamente)==========
         while(n_prendas_actual >= n_prendas_total)//agregar break si llega nuevo bit de inicio
         {
            //lectura_serial();
            //mantenimineto de temperatura?
         }
         
         
         //===================================================NO QUITAR==================================================
         //n_prendas_actual = 0;
         //lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
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
   if (temp <= TEM_DESEADA) {output_high(RESISTENCIA); lcd_gotoxy(14,1); printf(lcd_putc,"R:1");} //Se enciende la resistencia
   while (temp <= TEM_DESEADA) //Cada segundo, mide e imprime la temperatura
   {
      if (un_segundo)
      {
         temp = medir_temperatura();
         lcd_gotoxy(14,1); printf(lcd_putc,"R:1");
      }
      if (input(INFRAROJO)== 0 && *flag_motor_mec==1) //Si se detectó un objeto y está funcionando el motor del mecanismo
      {  
         break;      
      }
      if ((input(FIN_E1_UP) == 0 || input(FIN_E1_DOWN) == 0) && *flag_motor_elev==1) //Si el elevador llegó hasta arriba o hasta abajo y está funcionando el elevador***********
      {  
         break;      
      }
   }
   output_low(RESISTENCIA); //Apaga la resistencia
   lcd_gotoxy(14,1); printf(lcd_putc,"R:0");
}

void revisar_fines_carrera (int8 *flag_motor_elev)
{
   if (input(FIN_E1_UP) == 0){UPDOWN (PARO, flag_motor_elev); delay_ms(500); UPDOWN (ABAJO, flag_motor_elev);lcd_gotoxy(5,2); printf(lcd_putc,"U:1");}
   else if (input(FIN_E1_UP) == 1){lcd_gotoxy(5,2); printf(lcd_putc,"U:0");}
   if (input(FIN_E1_DOWN) == 0){UPDOWN (PARO, flag_motor_elev); delay_ms(500); UPDOWN (ARRIBA, flag_motor_elev);lcd_gotoxy(9,2); printf(lcd_putc,"D:1");}
   else if (input(FIN_E1_DOWN) == 1){lcd_gotoxy(9,2); printf(lcd_putc,"D:0");}
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
      set_pwm1_duty(680); // Revisar si es la velocidad correcta para el mecanismo======================================================
      lcd_gotoxy(6,1); printf(lcd_putc,"M:1");
      *flag_motor_mec = 1;
      
   }
   else
   {
      set_pwm1_duty(0);
      lcd_gotoxy(6,1); printf(lcd_putc,"M:0");
      *flag_motor_mec = 0;
   }
}

void lectura_serial (int8 *inicio_flag, int8 *paro_flag, int8 *tiempo, int8 *prendas)
{
         *inicio_flag = 1;
         *paro_flag = 0;
         *tiempo = 30;
         *prendas = 2;
}




