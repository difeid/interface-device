/*
Подключение заголовочного файла io.h, который в свою очередь подключит iom169.h
iom169.h - заголовочный файл, который ставит в соответствие константам PORTA и PORTB реальные адреса выводов конкретного контроллера (в нашем случае ATmega169)
Таким образом io.h позволяет работать с портами ввода-вывода
*/
#include <avr/io.h>

// Подключение заголовочного файла, который позволяет объявлять булевские переменные
#include <stdbool.h>

// Определение пользовательского типа данных byte
typedef unsigned char byte;

// Константы, определённые для обращения к выводам порта А
#define C1			(PINA & 0x01) /* 0-вой вывод порта А, на который приходит сигнал С1  */
#define C2			(PINA & 0x02) /* 1-вой вывод порта А, на который приходит сигнал С2  */
#define DATAIN		((PINA & 0x04) >> 2) /* 2-вой вывод порта А, на который приходит информация. Чтение информации из порта */
#define DATAOUT(x)	PORTA = (PORTA & ~(0x04)) | (x << 2) /* 2-вой вывод порта А, на который приходит информация. Ввод информации в порт */

// Символы сигналов связи варианта задания на курсовой проект #4
#define ATTENTION 217					// Внимание
#define ADDRESS 57						// Адрес
#define READY 237						// Готов
#define BUSY 253						// Занят
#define END_OF_TRANSFER_SEQUENCE 114	// Конец передачи
#define END_OF_RECEIVE 174				// Конец приёма
#define REPEAT_TRANSFER 154				// Повторить передачу

// Символы сигналов связи от Абонента
#define END_OF_WORK 1					// Конец работы
#define TRANSFER_ERROR 2				// Ошибка передачи

// Глобальные переменные
byte frameFromArbiter = 0;
bool abonentStateReceived = false;
bool frameCameFromArbiter = false;
bool AddressCameFromArbiter = false;
bool frameToAbonentWasSent = false;
bool requestToAbonentWasSent = false;
bool stateOfAbonentWasSentToArbiter = false;
bool DataMessageRefered = false;
int  abonentState = 0;

// Получаем побитово байт от Арбитра
void ReceiveFrameFromArbiter()
{
	static int C2Count = 0;
	static bool C1Came = false;
	static bool C2Out = true;

	if(C1) // Если пришёл сигнал С1,
	{
		C1Came = true; // запоминаем что сигнал С1 приходил
	}	

	if(C1Came) // Если сигнал С1 приходил
	{
		if(C2 && C2Out) // Если сигнал С2 пришёл,
		{
			frameFromArbiter = (frameFromArbiter & ~(1 << C2Count)) | (DATAIN << C2Count); // считываем очередной бит с информационной линии
			/*
				(1 << C2Count) даст нам байт с единицой, установленной на том бите, который мы должны установить
				~(1 << C2Count) инвертирование даст нам байт с нулём, установленным на том бите, который мы должны установить
				(frameFromArbiter & ~(1 << C2Count)) даст нам исходный frameFromArbiter с нулём, установленным на том бите, который мы должны установить
				
				(DATAIN << C2Count) даст нам бит со значением пришедшим по информационной линии, который установлен в байте  на том бите, который мы должны установить

				Итоговое поразрядное ИЛИ даст нам исходный frameFromArbiter со значением пришедшим по информационной линии, которое установится в frameFromArbiter  на номере бита, равном номеру сигнала С2
			*/


			C2Count++; // Считаем номер пришёдшего сигнала С2
			C2Out = false; // Запоминаем, что С2 пришёл, но ещё не уходил
		}
			
		if(C2Count == 8) // Если пришло 8 сигналов С2, значит мы приняли целиком байт
		{
			C2Count = 0; // Обнуляем количество сигналов С2
			C1Came = false; // Запоминаем что пришёдший после С1 байт уже обработан, ждём следующий С1
			C2Out = true;
			frameCameFromArbiter = true; // Запоминаем что байт пришёл
		}

		if(!C2){ // Если сигнал С2 ушёл
			C2Out = true; // Запоминаем, что сигнал С2 ушёл
		}
	}
}




// Ждём поступления от Арбитра адреса микроконтроллера
void WaitAddressFromArbiter()
{
	DDRA = 0x00; // Указываем микроконтроллеру настроить все выводы на приём информации
	static  bool attentionCame = false;
	ReceiveFrameFromArbiter(); // Получить байт от Арбитра

	if(frameCameFromArbiter) // Если байт от Арбитра пришёл
	{
		if(attentionCame) // Если сигнал Внимание уже приходил
		{
			if(frameFromArbiter == ADDRESS) // И пришедший байт равен адресу микроконтроллера
			{
				AddressCameFromArbiter = true; // Запоминаем, что адрес пришёл
			}
			attentionCame = false;
		}

		if(frameFromArbiter == ATTENTION) // Если пришёл байт равный сигналу Внимание
		{
			attentionCame = true; // Запоминаем, что приходил сигнал Внимание
		}
		frameCameFromArbiter = false; // Записываем, что мы уже обработали пришедший байт
	}
}

// Посылаем параллельно байт Абоненту
void SendFrameToAbonent(byte sendFrame)
{
	static bool C1Came = false;
	if(C1) // Если пришёл сигнал С1,
	{
		C1Came = true; // запоминаем что сигнал С1 приходил
	}	

	if(C1Came) // Если сигнал С1 приходил
	{
		PORTB = sendFrame; // Выдаём на выводы порта B байт
	}
	
	if(!C1 && C1Came) // Если сигнал С1 ушёл и приходил
	{
		frameToAbonentWasSent = true; // Запоминаем что байт был отослан
		C1Came = false;
	}

}

// Посылаем Абоненту запрос о его состоянии
void SendToAbonentRequestAboutAbonentState()
{
	DDRB = 0xFF; // Указываем микроконтроллеру настроить все выводы порта B на вывод информации
	SendFrameToAbonent(READY); // Посылаем байт ГОТОВ Абоненту
	
	if(frameToAbonentWasSent) // Если байт был послан
	{
		requestToAbonentWasSent = true; // Запоминаем, что запрос Абоненту был послан
	}
}




// Получаем от Абонента его состояние
void ReceiveFromAbonentAbonentState()
{
	
	DDRB = 0x00; // Указываем микроконтроллеру настроить все выводы порта B на приём информации
	static bool C1Came = false;
	if(C1) // Если пришёл сигнал С1,
	{
		C1Came = true; // запоминаем что сигнал С1 приходил
	}	

	if(C1Came) // Если сигнал С1 приходил
	{
		abonentState = PINB; // Выдаём на выводы порта B байт
	}
	
	if(!C1 && C1Came) // Если сигнал С1 ушёл и приходил
	{
		abonentStateReceived = true; // Запоминаем что получили состояние Абонента
		C1Came = false;
	}

}

// Посылаем Арбитру состояние Абонента
void SendToArbiterAbonentState(byte frame)
{
	DDRA = 0x04;
	static int C2Count = 0;
	static bool C1Came = false;
	static bool C2Out = true;

	if(C1) // Если пришёл сигнал С1,
	{
		C1Came = true; // запоминаем что сигнал С1 приходил
	}	

	if(C1Came) // Если сигнал С1 приходил
	{
		if(C2 && C2Out) // Если сигнал С2 пришёл,
		{
			DATAOUT((frame & (1 << C2Count)) >> C2Count); // Записываем очередной бит на информационную линию
			C2Count++; // Считаем номер пришёдшего сигнала С2
			C2Out = false; // Запоминаем, что С2 пришёл, но ещё не уходил
		}

		if(C2Count == 8)
		{
			C2Count = 0; // Обнуляем количество сигналов С2
			C1Came = false; // Запоминаем что пришёдший после С1 байт уже обработан, ждём следующий С1
			C2Out = true;
			stateOfAbonentWasSentToArbiter = true; // Запоминаем, что состояние Абонента было отослано Арбитру
		}

		if(!C2){ // Если сигнал С2 ушёл
			C2Out = true; // Запоминаем, что сигнал С2 ушёл
		}
	
		
	}
}

// Получаем информационное сообщение от Арбитра и пересылаем Абоненту
void ReceiveDataMessageFromArbiterAndReferToAbonent()
{
	DDRA = 0x00; // Указываем микроконтроллеру настроить все выводы на приём информации
	ReceiveFrameFromArbiter(); // Получить байт от Арбитра
	if(frameCameFromArbiter) // Если байт от АРбитра пришёл
	{
		SendFrameToAbonent(frameFromArbiter); // Посылаем байт Абоненту
		frameCameFromArbiter = false; // Запоминаем, что мы обработали байт, пришедший от Арбитра

		if(frameFromArbiter == END_OF_TRANSFER_SEQUENCE) // Если пришёдший байт совпадает с символом КОНЕЦ ПЕРЕДАЧИ
		{
			DataMessageRefered = true; // Запоминаем что информационное сообщение переслали
		}
	}
}



int main (void)
{
	
	while(1) // Запускаем бесконечный цикл
	{
		if(!AddressCameFromArbiter) // Если адрес МК ещё не пришёл от Арбитра,
		{
			WaitAddressFromArbiter(); // ждём
		}
		
		if(AddressCameFromArbiter) // Если от Арбитра пришёл адрес МК
		{
			SendToAbonentRequestAboutAbonentState(); // посылаем Абоненту запрос о его состоянии

			if(requestToAbonentWasSent) // Если запрос о состоянии Абонента был послан
			{
				ReceiveFromAbonentAbonentState(); // получаем ответ
			}
			
			if(abonentStateReceived) // Если получили состояние Абонента,
			{
				if(abonentState == END_OF_WORK) // и состояние равно КОНЕЦ РАБОТЫ,
				{
					SendToArbiterAbonentState(READY); // посылаем Арбитру сигнал ГОТОВ

					if(stateOfAbonentWasSentToArbiter) // Если состояние Абонента отослалось Арбитру
					{
						ReceiveDataMessageFromArbiterAndReferToAbonent(); // Начинаем принимать информационное сообщение от Арбитра и пересылать его Абоненту

						if(DataMessageRefered) // Если информационное сообщение было отослано
						{
							SendToAbonentRequestAboutAbonentState(); // Посылаем Абоненту запрос о его состоянии
				
							if(requestToAbonentWasSent) // Если запрос о состоянии Абонента был послан,
							{
								ReceiveFromAbonentAbonentState(); // ждём ответ
							}
			
							if(abonentStateReceived) // Если получили состояние Абонента,
							{
								if(abonentState == TRANSFER_ERROR) // Если состояние Абонента равно ОШИБКА ПЕРЕДАЧИ
								{
									SendToArbiterAbonentState(REPEAT_TRANSFER); // Посылаем Арбитру сигнал ПОВТОРИТЬ ПЕРЕДАЧУ
								}
								else // Если ошибки не произошло
								{
									SendToArbiterAbonentState(END_OF_RECEIVE); // Посылаем Арбитру сигнал КОНЕЦ ПРИЁМА
								}
							}
						}
					}
				}
				else // Если состояние не равно КОНЕЦ РАБОТЫ
				{
					SendToArbiterAbonentState(BUSY); // Посылаем Арбитру, что Абонент ЗАНЯТ
				}
			}
		}
	}
}
		
