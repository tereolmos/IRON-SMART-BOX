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
#define FIN_E2_UP PIN_B4// inecesario
#define FIN_E2_DOWN PIN_B5 // inecesario

//SALIDAS
#define E1_UP PIN_B6
#define E1_DOWN PIN_B7
#define E2_UP PIN_C0 // inecesario
#define E2_DOWN PIN_C1 // inecesario
#define RESISTENCIA PIN_C5

//AUXILIARES
#define ARRIBA 0
#define ABAJO 1
#define PARO 2
#define ON 1
#define OFF 0
#define CARGA 3036
#define TEM_DESEADA 31

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
void control_temperatura (void); 
void revisar_fines_carrera (void);
void start_elevadores (void);
void UPDOWN (int8,int8);
void control_pwm (int1);
void lectura_serial (int8 *, int8 *, int8 *, int8 *);



void main ()
{
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
   UPDOWN (1,PARO);
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
   
   //Variables
   int8 start = 0; //Viene de LABVIEW, boton de INICIO
   int8 stop = 0; //Viene de LABVIEW, boton PARO DE EMERGENCIA
   int8 n_prendas_total = 0; // #Prendas colocadas por el usuario
   int8 n_prendas_actual = 0; // #Prendas ya planchadas
   int8 tiempo = 0; // Tiempo de planchado por prenda deseado

   while (TRUE)
   {
      lectura_serial (&start, &stop, &tiempo, &n_prendas_total);
      if(start == 1)
      {
         control_temperatura();
         while(n_prendas_actual < n_prendas_total)
         {
            lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
            if  (input(INFRAROJO)== 1)//No hay prenda detectada
            {
               lcd_gotoxy(1,2); printf(lcd_putc,"P:0");
               control_pwm (ON); //Se enciende el motor del mecanismo giratorio
               while (input(INFRAROJO)== 1);//Si no hay prenda detectada no hace NADA ******Agregar mantenimiento de temperatura******=================
               lcd_gotoxy(1,2); printf(lcd_putc,"P:1");// el ciclo se rompe cuando detecta la pieza 
               control_pwm (OFF); //Se apaga el motor del mecanismo giratorio
               UPDOWN (1,ARRIBA);
               comenzar_conteo = 1;
       
               while (t_actual/2 < tiempo)
               {
                  lcd_gotoxy(16,2); printf(lcd_putc,"%i ",(int)t_actual/2);
                  revisar_fines_carrera ();
               }
               
               UPDOWN (1,PARO);
               //Agregar que regresen a la posicion inicial=============================================================================================
               t_actual = 0;
               comenzar_conteo = 0;
               n_prendas_actual++;
               //lcd_gotoxy(1,1); printf(lcd_putc,"\fFIN DE PLANCHADO\nPRENDA: %i",n_prendas_actual);
               lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
               delay_ms(1000); //La prenda avanza un poquito para ya no ser detectada por el infrarojo
               control_temperatura();
               control_pwm (ON);
            }//if  (input(INFRAROJO)== 0) 
         }//while(n_prendas_actual < n_prendas_total)
         n_prendas_actual = 0;
         lcd_gotoxy(13,2); printf(lcd_putc,"#:%i",n_prendas_actual);
         
         
         
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

void control_temperatura () 
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
   }
   output_low(RESISTENCIA); //Apaga la resistencia
   lcd_gotoxy(14,1); printf(lcd_putc,"R:0");
}

void revisar_fines_carrera ()
{
   //ELEVADOR 1
   if (input(FIN_E1_UP) == 0){UPDOWN (1,PARO); delay_ms(500); UPDOWN (1,ABAJO);lcd_gotoxy(5,2); printf(lcd_putc,"U:1");}
   else if (input(FIN_E1_UP) == 1){lcd_gotoxy(5,2); printf(lcd_putc,"U:0");}
   if (input(FIN_E1_DOWN) == 0){UPDOWN (1,PARO); delay_ms(500); UPDOWN (1,ARRIBA);lcd_gotoxy(9,2); printf(lcd_putc,"D:1");}
   else if (input(FIN_E1_DOWN) == 1){lcd_gotoxy(9,2); printf(lcd_putc,"D:0");}
}

void start_elevadores ()
{
   if (input(FIN_E1_UP) == 0 && input(FIN_E1_DOWN) == 0){UPDOWN (1,ARRIBA);}
}

void UPDOWN (int8 motor,int8 sentido)

{
   switch (motor)
   {
      case 1:
         if (sentido == ARRIBA){output_high(E1_UP); output_low(E1_DOWN);lcd_gotoxy(10,1); printf(lcd_putc,"M:U");}
         else if (sentido == ABAJO) {output_high(E1_DOWN); output_low(E1_UP);lcd_gotoxy(10,1); printf(lcd_putc,"M:D");}
         else if (sentido == PARO) {output_low(E1_DOWN); output_low(E1_UP);lcd_gotoxy(10,1); printf(lcd_putc,"M:P");}
      break;
      
      case 2:
         if (sentido == ARRIBA){output_high(E2_UP); output_low(E2_DOWN);lcd_gotoxy(1,2); printf(lcd_putc,"E2: SUBIENDO    ");}
         else if (sentido == ABAJO) {output_high(E2_DOWN); output_low(E2_UP);lcd_gotoxy(1,2); printf(lcd_putc,"E2: BAJANDO     ");}
         else if (sentido == PARO) {output_low(E2_DOWN); output_low(E2_UP); lcd_gotoxy(1,2); printf(lcd_putc,"E2: AAAAA        ");}
      break;
      
      default:
      break;
   }
}

void control_pwm (int1 motor_flag) 
{
   int16 pwm;
   if (motor_flag)
   {
      pwm=680;
      set_pwm1_duty(pwm);
      lcd_gotoxy(6,1); printf(lcd_putc,"M:1");
      
   }
   else
   {
      pwm=0;
      set_pwm1_duty(pwm);
      lcd_gotoxy(6,1); printf(lcd_putc,"M:0");
   }
}

void lectura_serial (int8 *inicio_flag, int8 *paro_flag, int8 *tiempo, int8 *prendas)
{
//!         char datos [];
//!         int i = 0;
         *inicio_flag = 1;
         *paro_flag = 0;
         *tiempo = 20;
         *prendas = 4;
         
//!         while (!kbhit);
//!         
//!         while(kbhit)
//!         {
//!            datos[i] = getc();
//!         }
//!         
//!         switch(dato)
//!         {
//!            case '1':
//!               output_high(LED);
//!            break;
//!            
//!            case '0':
//!               output_low(LED);
//!            break;
//!            
//!            default:
//!            break;
//!         }
      
}




