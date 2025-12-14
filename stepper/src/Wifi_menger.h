/**
 * @file Wifi_menger.h
 * @brief Menedżer połączenia WiFi, mDNS oraz WebSockets.
 */

#ifndef WIFI_MENAGER_H
#define WIFI_MENAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LEAmDNS.h>
#include "packets.h"
#include <MsgPack.h>

/**
 * @typedef on_message_callback
 * @brief Typ funkcji zwrotnej (callback) do obsługi przychodzących danych.
 * * @param payload Wskaźnik do surowych danych.
 * @param lenght Długość danych w bajtach.
 * @param type Typ wiadomości WebSocket (Tekst/Binarna).
 */
typedef void (*on_message_callback)(uint8_t* payload, size_t lenght, WStype_t type);

/**
 * @class WiFiMenager
 * @brief Klasa zarządzająca stosem sieciowym maszyny.
 * * Odpowiada za połączenie WiFi, rozgłaszanie usługi mDNS oraz
 * utrzymanie połączenia WebSocket z serwerem (PC).
 */
class WiFiMenager {
private:
    const char* ssid; ///< Nazwa sieci WiFi.
    const char* password; ///< Hasło do sieci WiFi.

    int port; ///< Port serwera WebSocket.
    IPAddress serverIP; ///< Adres IP serwera.
    WebSocketsClient client; ///< Klient biblioteki WebSockets.

    const char* server_name = "winding_server"; ///< Nazwa szukanej usługi serwera.
    const char* service_name = "winder"; ///< Nazwa własna usługi maszyny.
    const char* server_protocol = "tcp"; ///< Protokół mDNS.

    on_message_callback message_callback; ///< Wskaźnik na funkcję obsługi danych.

    unsigned long interval = 5000; ///< Interwał ponawiania połączenia (ms)
    
    /**
     * @brief Wewnętrzna obsługa zdarzeń biblioteki WebSockets.
     * Przekierowuje dane do message_callback.
     */
    void web_socket_events(WStype_t type, uint8_t* payload, size_t length);

public:
    /**
     * @brief Konstruktor menedżera WiFi.
     * @param ssid Nazwa sieci.
     * @param pass Hasło sieci.
     */
    WiFiMenager(char* ssid, char* pass);

    /**
     * @brief Ustawia funkcję callback wywoływaną po odebraniu danych.
     * @param ms Wskaźnik na funkcję.
     */
    void set_callback(on_message_callback ms);

    /**
     * @brief Inicjalizuje moduł WiFi i usługę mDNS.
     * @return true jeśli połączono z WiFi, false w przypadku błędu.
     */
    bool init();

    /**
     * @brief Wyszukuje serwer PC w sieci lokalnej używając mDNS.
     * @return true jeśli znaleziono serwer i pobrano IP.
     */
    bool find_server();

    /**
     * @brief Główna pętla obsługi sieci.
     * Musi być wywoływana cyklicznie. Obsługuje stos TCP/IP i mDNS.
     */
    void run();

    /**
     * @brief Sprawdza status połączenia WebSocket.
     * @return true jeśli połączono z serwerem.
     */
    bool is_connected();

    /**
     * @brief Wysyła strukturę statusu jako wiadomość binarną MsgPack.
     * * @param meassage Struktura danych do wysłania.
     * @param packer Referencja do obiektu pakującego MsgPack.
     * @return true jeśli wysłano pomyślnie.
     */
    bool send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer);
};

#endif