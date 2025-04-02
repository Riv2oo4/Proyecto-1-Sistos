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


class Contacto {
private:
    std::string nombreUsuario;
    EstadoUsuario estado;

public:
    // Constructores
    Contacto() : nombreUsuario(""), estado(EstadoUsuario::DESCONECTADO) {}
    Contacto(std::string nombreUsuario, EstadoUsuario estado) 
        : nombreUsuario(std::move(nombreUsuario)), estado(estado) {}
    
    // Getters
    const std::string& obtenerNombre() const { return nombreUsuario; }
    EstadoUsuario obtenerEstado() const { return estado; }
    
    // Setters
    void establecerNombre(const std::string& nombre) { nombreUsuario = nombre; }
    void establecerEstado(EstadoUsuario nuevoEstado) { estado = nuevoEstado; }
    
    wxString obtenerNombreFormateado() const {
        std::string indicadorEstado;
        switch (estado) {
            case EstadoUsuario::ACTIVO: indicadorEstado = "[+] "; break;
            case EstadoUsuario::OCUPADO: indicadorEstado = "[!] "; break;
            case EstadoUsuario::INACTIVO: indicadorEstado = "[~] "; break;
            case EstadoUsuario::DESCONECTADO: indicadorEstado = "[-] "; break;
        }
        return wxString(indicadorEstado + nombreUsuario);
    }
};

class VistaChat;
class VistaLogin;
class AplicacionMensajero;

class AplicacionMensajero : public wxApp {
public:
    virtual bool OnInit() override;
};

wxIMPLEMENT_APP(AplicacionMensajero);

class VistaChat : public wxFrame {
public:
    VistaChat(std::shared_ptr<websocket::stream<tcp::socket>> conexion, const std::string& nombreUsuario);
    ~VistaChat();

private:
    wxListBox* listaContactos;
    wxTextCtrl* panelHistorialChat;
    wxTextCtrl* campoEntradaMensaje;
    wxBitmapButton* botonEnviar;
    wxButton* botonAyuda;
    wxButton* botonInfoUsuario;
    wxBitmapButton* botonActualizar;
    wxChoice* selectorEstado;
    wxStaticText* etiquetaTituloChat;
    wxStaticText* etiquetaEstado;
    wxButton* botonCerrarSesion;

    
    // Red y estado
    std::shared_ptr<websocket::stream<tcp::socket>> conexion;
    std::string usuarioActual;
    std::string contactoActivo;
    bool estaEjecutando;
    std::mutex mutexDatosChat;
    EstadoUsuario estadoActualUsuario;

    // Almacenamiento de datos
    std::unordered_map<std::string, Contacto> directorioContactos;
    std::unordered_map<std::string, std::vector<std::string>> historialMensajes;
    
    // Manejadores de eventos UI
    void alEnviarMensaje(wxCommandEvent& evento);
    void alSeleccionarContacto(wxCommandEvent& evento);
    void alSolicitarInfoUsuario(wxCommandEvent& evento);
    void alActualizarContactos(wxCommandEvent& evento);
    void alCambiarEstado(wxCommandEvent& evento);
    void alMostrarAyuda(wxCommandEvent& evento);
    void alCerrarSesion(wxCommandEvent& evento);

    
    // Operaciones de red
    void obtenerListaUsuarios();
    void obtenerHistorialChat();
    void iniciarEscuchaMensajes();
    bool verificarConexion();
    bool reconectar();
    
    // Constructores de mensajes de protocolo
    std::vector<uint8_t> crearSolicitudListaUsuarios();
    std::vector<uint8_t> crearSolicitudInfoUsuario(const std::string& nombreUsuario);
    std::vector<uint8_t> crearSolicitudActualizacionEstado(EstadoUsuario nuevoEstado);
    std::vector<uint8_t> crearSolicitudEnvioMensaje(const std::string& destinatario, const std::string& mensaje);
    std::vector<uint8_t> crearSolicitudHistorial(const std::string& contactoChat);
    
    // Manejadores de mensajes de protocolo
    void manejarMensajeError(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeListaUsuarios(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeInfoUsuario(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeNuevoUsuario(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeCambioEstado(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeChat(const std::vector<uint8_t>& datosMensaje);
    void manejarMensajeHistorialChat(const std::vector<uint8_t>& datosMensaje);
    
    void actualizarListaContactos();
    void actualizarVistaEstado();
    bool puedeEnviarMensajes() const;
    bool estaConectado();
};

class VistaLogin: public wxFrame{

};