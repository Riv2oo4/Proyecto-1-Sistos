#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/choice.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <algorithm>
#include <wx/statline.h>
#include <wx/artprov.h>
#include <wx/bmpbuttn.h>

// Alias de espacios de nombres para un código más limpio
namespace red = boost::asio;
namespace bestia = boost::beast;
namespace websocket = bestia::websocket;
using tcp = red::ip::tcp;

// Tipos de mensajes del protocolo
enum TipoMensajeProtocolo : uint8_t {
    // Mensajes del cliente al servidor
    MSG_CLIENTE_SOLICITAR_USUARIOS = 1,
    MSG_CLIENTE_OBTENER_INFO_USUARIO = 2,
    MSG_CLIENTE_ACTUALIZAR_ESTADO = 3,
    MSG_CLIENTE_ENVIAR_MENSAJE = 4,
    MSG_CLIENTE_SOLICITAR_HISTORIAL = 5,

    // Mensajes del servidor al cliente
    MSG_SERVIDOR_ERROR = 50,
    MSG_SERVIDOR_LISTA_USUARIOS = 51,
    MSG_SERVIDOR_INFO_USUARIO = 52,
    MSG_SERVIDOR_USUARIO_CONECTADO = 53,
    MSG_SERVIDOR_CAMBIO_ESTADO = 54,
    MSG_SERVIDOR_NUEVO_MENSAJE = 55,
    MSG_SERVIDOR_HISTORIAL_CHAT = 56
};

// Códigos de error del servidor
enum CodigoError : uint8_t {
    ERR_USUARIO_NO_ENCONTRADO = 1,
    ERR_ESTADO_INVALIDO = 2,
    ERR_MENSAJE_VACIO = 3,
    ERR_DESTINATARIO_DESCONECTADO = 4
};

// Estado del usuario
enum class EstadoUsuario : uint8_t {
    DESCONECTADO = 0,
    ACTIVO = 1,
    OCUPADO = 2,
    INACTIVO = 3
};

std::string obtenerNombreEstado(EstadoUsuario estado) {
    switch (estado) {
        case EstadoUsuario::ACTIVO: return "ACTIVO";
        case EstadoUsuario::OCUPADO: return "OCUPADO";
        case EstadoUsuario::INACTIVO: return "INACTIVO";
        case EstadoUsuario::DESCONECTADO: return "DESCONECTADO";
        default: return "DESCONOCIDO";
    }
}