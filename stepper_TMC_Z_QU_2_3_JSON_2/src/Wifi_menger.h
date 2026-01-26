/**
 * @file Wifi_menger.h
 * @brief Menadżer połączenia WiFi, WebSocket i serwera HTTP.
 */
#ifndef WIFI_MENAGER_H
#define WIFI_MENAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <LEAmDNS.h>
#include "packets.h"
#include <LittleFS.h>     
#include <ArduinoJson.h>

/**
 * @brief Definicja typu funkcji zwrotnej (callback) do obsługi wiadomości WebSocket.
 */
typedef void (*on_message_callback)(uint8_t num, uint8_t* payload, size_t lenght, WStype_t type);
/**
 * @class WiFiMenager
 * @brief Klasa zarządzająca całą komunikacją sieciową.
 * * Odpowiada za postawienie Access Pointa lub połączenie ze stacją,
 * obsługę serwera plików (LittleFS) oraz komunikację w czasie rzeczywistym przez WebSockety.
 */
class WiFiMenager {
private:

    String ssid; 
    String password; 

    int port_webSocket = 81; // porty standardowe
    int port_http = 80;
    int active_clients = 0; // ile osob jest połączonych za pomocą panela operatorskeigo 

    const char* host_name = "winder";
    WebServer server;
    on_message_callback message_callback; // tu trzymamy funkcje wywoływaną jak cos przyjdzie
    WebSocketsServer websocket;
    /**
     * @brief Wewnętrzna obsługa zdarzeń WebSocket (podłączenie, rozłączenie, dane).
     */
    void web_socket_events(uint8_t num ,WStype_t type, uint8_t* payload, size_t length);

public:

    /** @brief Flaga informująca, czy konfiguracja uległa zmianie i trzeba przeładować. */
    bool config_changed = false;

    /**
     * @brief Konstruktor z gotowymi danymi logowania.
     * @param ssid Nazwa sieci WiFi.
     * @param pass Hasło do sieci WiFi.
     */
    WiFiMenager(char* ssid, char* pass);

    /**
     * @brief Konstruktor domyślny (dane wczytane z pliku lub puste).
     */
    WiFiMenager();

    /**
     * @brief Ustawia funkcję callback wywoływaną po otrzymaniu danych.
     * @param ms Wskaźnik do funkcji obsługującej dane.
     */
    void set_callback(on_message_callback ms);

    /**
     * @brief Inicjalizuje system plików i połączenie sieciowe.
     * @return true Jeśli udało się połączyć lub postawić sieć.
     */
    bool init();

    /**
     * @brief Główna pętla obsługi sieci (musi być w loop()).
    */
    void run();

    /**
     * @brief Rozpoznaje typ MIME na podstawie rozszerzenia pliku.
     */
    String get_content_type(String filename);

    /**
     * @brief Obsługuje żądanie odczytu pliku z serwera HTTP.
     */
    bool handle_file_read(String path);

    /** @brief Obsługa endpointu API do pobierania konfiguracji (GET). */
    void handle_config_get();

    /** @brief Obsługa endpointu API do zapisu konfiguracji (POST). */
    void handle_config_post();

    /**
     * @brief Uruchamia kreator połączenia przez port szeregowy.
     * Pozwala wpisać SSID i hasło w terminalu, jeśli WiFi nie działa.
     * @return true Jeśli konfiguracja została zapisana.
     */
    bool serial_wizard();
    
    /**
     * @brief Pobiera aktualne dane logowania WiFi.
     * @param ssid Referencja do stringa na SSID.
     * @param pass Referencja do stringa na hasło.
     * @return true Jeśli dane istnieją.
     */
    bool wifi_get(String &ssid, String &pass);
    
    /**
     * @brief Wczytuje ustawienia silników z pliku JSON.
     */
    bool load_config(float &sx, float &sy, float &sz, float &st_mm, float &st_rot, float &st_br);

    /** @brief Obsługa ręcznego sterowania maszyną (JOG) przez HTTP. */
    void handle_jog();
    
    /** @brief Sprawdza czy jesteśmy połączeni z siecią. */
    bool isCon();

    void send_stop();
    
    void reconnect();

    /**
     * @brief Wysyła dane w formacie JSON do klienta.
     * @param client_num ID klienta.
     * @param data Struktura statusu maszyny.
     * @return true Jeśli wysłano pomyślnie.
     */
    bool send_json(uint8_t client_num, MachineStatus& data);
};

#endif