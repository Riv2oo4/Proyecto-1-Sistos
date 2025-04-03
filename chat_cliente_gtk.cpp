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
    // Componentes de UI
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
    
    // Métodos de actualización de UI
    void actualizarListaContactos();
    void actualizarVistaEstado();
    bool puedeEnviarMensajes() const;
    bool estaConectado();
};


class VistaLogin : public wxFrame {
    public:
        VistaLogin() 
            : wxFrame(nullptr, wxID_ANY, "Inicio de Sesión del Chat", wxDefaultPosition, wxSize(450, 320)) {
            
            // Configurar tema oscuro
            SetBackgroundColour(wxColour(32, 32, 32)); // Fondo oscuro para la ventana principal
            
            wxPanel* panelPrincipal = new wxPanel(this);
            panelPrincipal->SetBackgroundColour(wxColour(32, 32, 32)); // Fondo oscuro para el panel
            
            wxBoxSizer* diseñoPrincipal = new wxBoxSizer(wxVERTICAL);
            
            // Título de la ventana con estilo
            wxStaticText* tituloLogin = new wxStaticText(panelPrincipal, wxID_ANY, "CHAT", 
                                                    wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
            tituloLogin->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro para contraste
            wxFont fuenteTitulo = tituloLogin->GetFont();
            fuenteTitulo.SetPointSize(fuenteTitulo.GetPointSize() + 5);
            fuenteTitulo.SetWeight(wxFONTWEIGHT_BOLD);
            tituloLogin->SetFont(fuenteTitulo);
            diseñoPrincipal->Add(tituloLogin, 0, wxEXPAND | wxALL, 20);
            
            // Entrada de nombre de usuario
            wxBoxSizer* diseñoUsuario = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* etiquetaUsuario = new wxStaticText(panelPrincipal, wxID_ANY, "Usuario:", 
                                                        wxDefaultPosition, wxSize(120, -1), wxALIGN_RIGHT);
            etiquetaUsuario->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            etiquetaUsuario->SetMinSize(wxSize(120, -1));
            diseñoUsuario->Add(etiquetaUsuario, 0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
            
            campoUsuario = new wxTextCtrl(panelPrincipal, wxID_ANY, "", wxDefaultPosition, wxDefaultSize);
            campoUsuario->SetBackgroundColour(wxColour(60, 60, 60)); // Fondo más claro para campo de texto
            campoUsuario->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            diseñoUsuario->Add(campoUsuario, 1, wxALL | wxEXPAND, 10);
            diseñoPrincipal->Add(diseñoUsuario, 0, wxEXPAND);
    
            // Entrada de dirección del servidor (con etiqueta "IP:")
            wxBoxSizer* diseñoDireccion = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* etiquetaDireccion = new wxStaticText(panelPrincipal, wxID_ANY, "IP:", 
                                                        wxDefaultPosition, wxSize(120, -1), wxALIGN_RIGHT);
            etiquetaDireccion->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            diseñoDireccion->Add(etiquetaDireccion, 0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
            
            campoDireccionServidor = new wxTextCtrl(panelPrincipal, wxID_ANY, "127.0.0.1");
            campoDireccionServidor->SetBackgroundColour(wxColour(60, 60, 60)); // Fondo más claro
            campoDireccionServidor->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            diseñoDireccion->Add(campoDireccionServidor, 1, wxALL | wxEXPAND, 10);
            diseñoPrincipal->Add(diseñoDireccion, 0, wxEXPAND);
    
            // Entrada de puerto del servidor
            wxBoxSizer* diseñoPuerto = new wxBoxSizer(wxHORIZONTAL);
            wxStaticText* etiquetaPuerto = new wxStaticText(panelPrincipal, wxID_ANY, "Puerto Servidor:", 
                                                      wxDefaultPosition, wxSize(120, -1), wxALIGN_RIGHT);
            etiquetaPuerto->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            diseñoPuerto->Add(etiquetaPuerto, 0, wxALL | wxALIGN_CENTER_VERTICAL, 10);
            
            campoPuertoServidor = new wxTextCtrl(panelPrincipal, wxID_ANY, "8080");
            campoPuertoServidor->SetBackgroundColour(wxColour(60, 60, 60)); // Fondo más claro
            campoPuertoServidor->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
            diseñoPuerto->Add(campoPuertoServidor, 1, wxALL | wxEXPAND, 10);
            diseñoPrincipal->Add(diseñoPuerto, 0, wxEXPAND);
    
            // Espacio entre campos y botones
            diseñoPrincipal->AddSpacer(15);
            
            // Botones de Conectar y Cancelar en fila horizontal
            wxBoxSizer* diseñoBotones = new wxBoxSizer(wxHORIZONTAL);
            
            // Botón de conexión
            wxButton* botonConectar = new wxButton(panelPrincipal, wxID_ANY, "Conectar");
            botonConectar->SetBackgroundColour(wxColour(70, 130, 180)); // Azul acero
            botonConectar->SetForegroundColour(wxColour(255, 255, 255)); // Texto blanco
            diseñoBotones->Add(botonConectar, 1, wxALL, 10);
            
            // Botón de cancelar
            wxButton* botonCancelar = new wxButton(panelPrincipal, wxID_ANY, "Cancelar");
            botonCancelar->SetBackgroundColour(wxColour(169, 68, 66)); // Rojo oscuro
            botonCancelar->SetForegroundColour(wxColour(255, 255, 255)); // Texto blanco
            diseñoBotones->Add(botonCancelar, 1, wxALL, 10);
            
            diseñoPrincipal->Add(diseñoBotones, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
    
            // Etiqueta de estado
            etiquetaEstadoConexion = new wxStaticText(panelPrincipal, wxID_ANY, "", 
                                              wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
            etiquetaEstadoConexion->SetForegroundColour(wxColour(255, 99, 71)); // Rojo tomate
            diseñoPrincipal->Add(etiquetaEstadoConexion, 0, wxALL | wxEXPAND, 10);
    
            // Enlace de eventos
            botonConectar->Bind(wxEVT_BUTTON, &VistaLogin::alHacerClicEnConectar, this);
            botonCancelar->Bind(wxEVT_BUTTON, &VistaLogin::alHacerClicEnCancelar, this);
            
            // Establecer sizer principal y centrar la ventana en pantalla
            panelPrincipal->SetSizer(diseñoPrincipal);
            diseñoPrincipal->Fit(this);
            Centre();
        }
    
    private:
        wxTextCtrl* campoUsuario;
        wxTextCtrl* campoDireccionServidor;
        wxTextCtrl* campoPuertoServidor;
        wxStaticText* etiquetaEstadoConexion;
    
        void alHacerClicEnConectar(wxCommandEvent& evento) {
            // Obtener información de conexión desde los campos de entrada
            std::string nombreUsuario = campoUsuario->GetValue().ToStdString();
            std::string direccionServidor = campoDireccionServidor->GetValue().ToStdString();
            std::string puertoServidor = campoPuertoServidor->GetValue().ToStdString();
            
            // Validar entradas
            if (nombreUsuario.empty()) {
                etiquetaEstadoConexion->SetLabel("Error: El nombre de usuario no puede estar vacío");
                return;
            }
            
            if (nombreUsuario == "~") {
                etiquetaEstadoConexion->SetLabel("Error: '~' está reservado para el chat general");
                return;
            }
            
            if (direccionServidor.empty()) {
                etiquetaEstadoConexion->SetLabel("Error: La dirección del servidor no puede estar vacía");
                return;
            }
            
            if (puertoServidor.empty()) {
                etiquetaEstadoConexion->SetLabel("Error: El puerto del servidor no puede estar vacío");
                return;
            }
            
            etiquetaEstadoConexion->SetLabel("Conectando...");
    
            // Crear conexión en un hilo separado para evitar bloquear la UI
            std::thread([this, nombreUsuario, direccionServidor, puertoServidor]() {
                try {
                    std::cout << "El cliente se está conectando al servidor: " << direccionServidor << ":" << puertoServidor << std::endl;
    
                    // Inicializar componentes de red
                    red::io_context contextoIO;
                    tcp::resolver resolvedor(contextoIO);
                    auto puntosFinal = resolvedor.resolve(direccionServidor, puertoServidor);
                    
                    std::cout << "Dirección resuelta correctamente. Estableciendo conexión TCP..." << std::endl;
            
                    // Conectar socket
                    tcp::socket socket(contextoIO);
                    red::connect(socket, puntosFinal);
                    
                    std::cout << "Conexión TCP establecida. Inicializando protocolo WebSocket..." << std::endl;
    
                    // Crear WebSocket
                    auto conexionWS = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
                    conexionWS->set_option(websocket::stream_base::timeout::suggested(bestia::role_type::client));
    
                    // Preparar handshake WebSocket
                    std::string anfitrion = direccionServidor;
                    std::string objetivo = "/?name=" + nombreUsuario;
                    
                    std::cout << "Iniciando autenticación WebSocket como usuario " << anfitrion 
                             << " con servidor: " << objetivo << std::endl;
            
                    // Realizar handshake
                    conexionWS->handshake(anfitrion, objetivo);
                    std::cout << "Autenticación WebSocket completada exitosamente!" << std::endl;
            
                    // Cambiar a ventana de chat en conexión exitosa
                    wxGetApp().CallAfter([this, conexionWS, nombreUsuario]() {
                        VistaChat* ventanaChat = new VistaChat(conexionWS, nombreUsuario);
                        ventanaChat->Show(true);
                        Close();
                    });
                } 
                catch (const bestia::error_code& ec) {
                    // Manejar errores de red
                    wxGetApp().CallAfter([this, ec]() {
                        std::string msgError = "Error de conexión: " + ec.message();
                        etiquetaEstadoConexion->SetLabel("Error: " + msgError);
                        std::cerr << msgError << std::endl;
                    });
                }
                catch (const std::exception& e) {
                    // Manejar excepciones generales
                    wxGetApp().CallAfter([this, e]() {
                        std::string msgError = e.what();
                        etiquetaEstadoConexion->SetLabel("Error: " + msgError);
                        std::cerr << "Excepción: " << msgError << std::endl;
                    });
                }
                catch (...) {
                    // Manejar excepciones desconocidas
                    wxGetApp().CallAfter([this]() {
                        etiquetaEstadoConexion->SetLabel("Error: Excepción desconocida durante la conexión");
                        std::cerr << "Error desconocido durante la conexión" << std::endl;
                    });
                }
            }).detach();
        }
        
        // Manejador para el botón Cancelar
        void alHacerClicEnCancelar(wxCommandEvent& evento) {
            Close(true);
        }
    };


bool AplicacionMensajero::OnInit() {
    VistaLogin* pantallaLogin = new VistaLogin();
    pantallaLogin->Show(true);
    return true;
}

VistaChat::VistaChat(std::shared_ptr<websocket::stream<tcp::socket>> conexion, const std::string& nombreUsuario)
    : wxFrame(nullptr, wxID_ANY, "CHAT - " + nombreUsuario, wxDefaultPosition, wxSize(900, 850)), 
      conexion(conexion), 
      usuarioActual(nombreUsuario),
      estaEjecutando(true),
      estadoActualUsuario(EstadoUsuario::ACTIVO) {

    SetBackgroundColour(wxColour(32, 32, 32)); 
    
    // Inicializar contactos con chat general y usuario actual
    directorioContactos.insert({"~", Contacto("Chat General", EstadoUsuario::ACTIVO)});
    
    // Crear UI
    wxPanel* panelPrincipal = new wxPanel(this);
    panelPrincipal->SetBackgroundColour(wxColour(32, 32, 32)); 
    
    wxBoxSizer* diseñoPrincipal = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* panelIzquierdo = new wxBoxSizer(wxVERTICAL);

    etiquetaTituloChat = new wxStaticText(panelPrincipal, wxID_ANY, "Chat con: Chat General");
    etiquetaTituloChat->SetForegroundColour(wxColour(200, 200, 200)); 
    wxFont fuenteTitulo = etiquetaTituloChat->GetFont();
    fuenteTitulo.SetPointSize(fuenteTitulo.GetPointSize() + 2);
    fuenteTitulo.SetWeight(wxFONTWEIGHT_BOLD);
    etiquetaTituloChat->SetFont(fuenteTitulo);
    panelIzquierdo->Add(etiquetaTituloChat, 0, wxALL, 10);

    // Visualización del historial de chat con estilo oscuro
    panelHistorialChat = new wxTextCtrl(panelPrincipal, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                     wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    panelHistorialChat->SetBackgroundColour(wxColour(45, 45, 45)); 
    panelHistorialChat->SetForegroundColour(wxColour(220, 220, 220)); 
    panelIzquierdo->Add(panelHistorialChat, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    // Entrada de mensaje y botón de enviar con estilo oscuro
    wxBoxSizer* diseñoEntradaMensaje = new wxBoxSizer(wxHORIZONTAL);
    campoEntradaMensaje = new wxTextCtrl(panelPrincipal, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                      wxTE_PROCESS_ENTER);
    campoEntradaMensaje->SetBackgroundColour(wxColour(60, 60, 60)); 
    campoEntradaMensaje->SetForegroundColour(wxColour(220, 220, 220)); 
    diseñoEntradaMensaje->Add(campoEntradaMensaje, 1, wxALL, 10);

    botonEnviar = new wxBitmapButton(panelPrincipal, wxID_ANY, 
        wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_BUTTON));
    botonEnviar->SetToolTip("Enviar mensaje");
    botonEnviar->SetBackgroundColour(wxColour(70, 130, 180)); 
    diseñoEntradaMensaje->Add(botonEnviar, 0, wxTOP | wxRIGHT | wxBOTTOM, 10);
    
    panelIzquierdo->Add(diseñoEntradaMensaje, 0, wxEXPAND);

    // Panel derecho - contactos y estado
    wxBoxSizer* panelDerecho = new wxBoxSizer(wxVERTICAL);

    wxStaticText* tituloSeccionContactos = new wxStaticText(panelPrincipal, wxID_ANY, "Contactos y Estado");
    tituloSeccionContactos->SetForegroundColour(wxColour(200, 200, 200));
    wxFont fuenteTituloSeccion = tituloSeccionContactos->GetFont();
    fuenteTituloSeccion.SetPointSize(fuenteTituloSeccion.GetPointSize() + 1);
    fuenteTituloSeccion.SetWeight(wxFONTWEIGHT_BOLD);
    tituloSeccionContactos->SetFont(fuenteTituloSeccion);
    panelDerecho->Add(tituloSeccionContactos, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 10);

    wxBoxSizer* diseñoEstado = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* etiquetaSelectorEstado = new wxStaticText(panelPrincipal, wxID_ANY, "Estado:");
    etiquetaSelectorEstado->SetForegroundColour(wxColour(200, 200, 200));
    diseñoEstado->Add(etiquetaSelectorEstado, 0, wxALL | wxALIGN_CENTER_VERTICAL, 10);

    wxString opcionesEstado[] = {"Activo", "Ocupado"};
    selectorEstado = new wxChoice(panelPrincipal, wxID_ANY, wxDefaultPosition, wxDefaultSize, 3, opcionesEstado);
    selectorEstado->SetBackgroundColour(wxColour(60, 60, 60));
    selectorEstado->SetForegroundColour(wxColour(220, 220, 220));
    selectorEstado->SetSelection(0);
    diseñoEstado->Add(selectorEstado, 1, wxALL, 10);

    panelDerecho->Add(diseñoEstado, 0, wxEXPAND);

    etiquetaEstado = new wxStaticText(panelPrincipal, wxID_ANY, "Estado actual: ACTIVO");
    etiquetaEstado->SetForegroundColour(wxColour(50, 205, 50));
    panelDerecho->Add(etiquetaEstado, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 10);

    wxStaticLine* lineaSeparadora = new wxStaticLine(panelPrincipal, wxID_ANY, wxDefaultPosition, 
                                                wxDefaultSize, wxLI_HORIZONTAL);
    panelDerecho->Add(lineaSeparadora, 0, wxEXPAND | wxALL, 10);

    wxBoxSizer* contactosHeader = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* etiquetaTituloContactos = new wxStaticText(panelPrincipal, wxID_ANY, "Contactos:");
    etiquetaTituloContactos->SetForegroundColour(wxColour(200, 200, 200));
    etiquetaTituloContactos->SetFont(fuenteTituloSeccion);
    contactosHeader->Add(etiquetaTituloContactos, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

    botonActualizar = new wxBitmapButton(panelPrincipal, wxID_ANY, 
        wxArtProvider::GetBitmap(wxART_REFRESH, wxART_BUTTON, wxSize(16, 16)));
    botonActualizar->SetToolTip("Actualizar lista de contactos");
    botonActualizar->SetBackgroundColour(wxColour(60, 60, 60));
    contactosHeader->Add(botonActualizar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    panelDerecho->Add(contactosHeader, 0, wxEXPAND | wxTOP, 10);

    listaContactos = new wxListBox(panelPrincipal, wxID_ANY);
    listaContactos->SetBackgroundColour(wxColour(45, 45, 45));
    listaContactos->SetForegroundColour(wxColour(220, 220, 220));
    listaContactos->SetFont(fuenteTituloSeccion);
    panelDerecho->Add(listaContactos, 1, wxALL | wxEXPAND, 10);

    // Botones de gestión de contactos con estilo oscuro
    wxBoxSizer* diseñoBotonesContacto = new wxBoxSizer(wxHORIZONTAL);

    botonAyuda = new wxButton(panelPrincipal, wxID_ANY, "Ayuda");
    botonAyuda->SetBackgroundColour(wxColour(100, 100, 180));
    botonAyuda->SetForegroundColour(wxColour(255, 255, 255));
    diseñoBotonesContacto->Add(botonAyuda, 1, wxALL, 5);

    botonInfoUsuario = new wxButton(panelPrincipal, wxID_ANY, "Info");
    botonInfoUsuario->SetBackgroundColour(wxColour(70, 130, 180));
    botonInfoUsuario->SetForegroundColour(wxColour(255, 255, 255));
    diseñoBotonesContacto->Add(botonInfoUsuario, 1, wxALL, 5);

    botonCerrarSesion = new wxButton(panelPrincipal, wxID_ANY, "Salir");
    botonCerrarSesion->SetBackgroundColour(wxColour(169, 68, 66)); 
    botonCerrarSesion->SetForegroundColour(wxColour(255, 255, 255)); 
    diseñoBotonesContacto->Add(botonCerrarSesion, 1, wxALL, 5);
    panelDerecho->Add(diseñoBotonesContacto, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 10);

    diseñoPrincipal->Add(panelIzquierdo, 2, wxEXPAND | wxALL, 10);
    
    // Agregar un separador vertical entre paneles
    wxStaticLine* separadorVertical = new wxStaticLine(panelPrincipal, wxID_ANY, wxDefaultPosition, 
                                                   wxDefaultSize, wxLI_VERTICAL);
    diseñoPrincipal->Add(separadorVertical, 0, wxEXPAND | wxTOP | wxBOTTOM, 20);
    
    diseñoPrincipal->Add(panelDerecho, 1, wxEXPAND | wxALL, 10);

    // Vincular eventos
    botonEnviar->Bind(wxEVT_BUTTON, &VistaChat::alEnviarMensaje, this);
    campoEntradaMensaje->Bind(wxEVT_TEXT_ENTER, &VistaChat::alEnviarMensaje, this);
    botonAyuda->Bind(wxEVT_BUTTON, &VistaChat::alMostrarAyuda, this);
    botonInfoUsuario->Bind(wxEVT_BUTTON, &VistaChat::alSolicitarInfoUsuario, this);
    botonActualizar->Bind(wxEVT_BUTTON, &VistaChat::alActualizarContactos, this);
    listaContactos->Bind(wxEVT_LISTBOX, &VistaChat::alSeleccionarContacto, this);
    selectorEstado->Bind(wxEVT_CHOICE, &VistaChat::alCambiarEstado, this);
    botonCerrarSesion->Bind(wxEVT_BUTTON, &VistaChat::alCerrarSesion, this);


    // Establecer diseño
    panelPrincipal->SetSizer(diseñoPrincipal);
    diseñoPrincipal->Fit(this);

    // Iniciar operaciones de red
    iniciarEscuchaMensajes();
    obtenerListaUsuarios();
    actualizarListaContactos();
    
    // Seleccionar chat general por defecto
    listaContactos->SetSelection(listaContactos->FindString("[+] Chat General"));
    contactoActivo = "~";
    etiquetaTituloChat->SetLabel("Chat con: Chat General");

    historialMensajes.clear();
    panelHistorialChat->Clear();



    
    // Centrar la ventana en la pantalla
    Centre();
}

void VistaChat::actualizarVistaEstado() {
    wxString textoEstado;
    wxColour colorEstado;
    
    switch (estadoActualUsuario) {
        case EstadoUsuario::ACTIVO:
            textoEstado = "ACTIVO";
            colorEstado = wxColour(50, 205, 50);  
            break;
        case EstadoUsuario::OCUPADO:
            textoEstado = "OCUPADO";
            colorEstado = wxColour(255, 99, 71);  
            break;
        case EstadoUsuario::INACTIVO:
            textoEstado = "INACTIVO";
            colorEstado = wxColour(255, 215, 0);  
            break;
        case EstadoUsuario::DESCONECTADO:
            textoEstado = "DESCONECTADO";
            colorEstado = wxColour(169, 169, 169);  
            break;
    }
    
    etiquetaEstado->SetLabel("Estado actual: " + textoEstado);
    etiquetaEstado->SetForegroundColour(colorEstado);
    
    // Actualizar estado del usuario en lista de contactos
    auto it = directorioContactos.find(usuarioActual);
    if (it != directorioContactos.end()) {
        it->second.establecerEstado(estadoActualUsuario);
    }
    
    actualizarListaContactos();
}

VistaChat::~VistaChat() {
    estaEjecutando = false;
    historialMensajes.clear();  
    directorioContactos.clear();  

    try {
        conexion->close(websocket::close_code::normal);
    } catch (...) {
    }
}
void VistaChat::alCerrarSesion(wxCommandEvent&) {
    estaEjecutando = false;

    try {
        if (conexion && conexion->is_open()) {
            conexion->close(websocket::close_code::normal);
        }
    } catch (...) {}

    historialMensajes.clear();
    directorioContactos.clear();

    wxGetApp().CallAfter([]() {
        wxMessageBox("Sesión cerrada correctamente", "Cierre de sesión", wxOK | wxICON_INFORMATION);

        VistaLogin* login = new VistaLogin();
        login->Show(true);
    });

    Close();
}

    class DialogoAyuda;
    class DialogoAyuda : public wxDialog {
        public:
            DialogoAyuda(wxWindow* parent)
                : wxDialog(parent, wxID_ANY, "Manual de Ayuda", wxDefaultPosition, wxSize(580, 480),
                           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        
                SetBackgroundColour(wxColour(32, 32, 32));  
        
                wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
                wxString contenido =
                    "MANUAL DE USO DEL CHAT\n\n"
                    "1. CONTACTOS\n"
                    "   - Los contactos disponibles aparecen en la lista de la derecha\n"
                    "   - Los estados se muestran con los siguientes símbolos:\n"
                    "     [+] Usuario Activo\n"
                    "     [!] Usuario Ocupado\n"
                    "     [~] Usuario INACTIVO\n"
                    "     [-] Usuario Desconectado\n\n"
                    "2. CHAT\n"
                    "   - Seleccione un contacto para iniciar un chat\n"
                    "   - Escriba su mensaje y presione el botón de la flecha para enviar\n"
                    "   - Use el chat general para mensajes públicos\n\n"
                    "3. ESTADO\n"
                    "   - Puede cambiar su estado usando el selector en la parte superior derecha\n"
                    "   - Sus mensajes no se enviarán si su estado es OCUPADO\n\n"
                    "4. INFORMACIÓN\n"
                    "   - Presione el botón 'Info' para ver detalles de un contacto seleccionado\n"
                    "   - Presione el botón de actualizar para refrescar la lista de contactos\n";
        
                wxTextCtrl* areaTexto = new wxTextCtrl(this, wxID_ANY, contenido,
                    wxDefaultPosition, wxSize(550, 400),
                    wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        
                areaTexto->SetBackgroundColour(wxColour(45, 45, 45));
                areaTexto->SetForegroundColour(wxColour(220, 220, 220));
                areaTexto->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        
                sizer->Add(areaTexto, 1, wxALL | wxEXPAND, 10);
        
                wxButton* cerrar = new wxButton(this, wxID_OK, "Cerrar");
                cerrar->SetBackgroundColour(wxColour(70, 130, 180));
                cerrar->SetForegroundColour(*wxWHITE);
                sizer->Add(cerrar, 0, wxALL | wxALIGN_CENTER, 10);
        
                SetSizerAndFit(sizer);
                Centre();
            }
        };
        
        
    

    void VistaChat::alMostrarAyuda(wxCommandEvent&) {
        std::cout << "Mostrando diálogo de ayuda..." << std::endl;
        DialogoAyuda* dlg = new DialogoAyuda(this);
        std::cout << "Diálogo creado, mostrando modal..." << std::endl;
        dlg->ShowModal();
        std::cout << "Modal mostrado, destruyendo diálogo..." << std::endl;
        dlg->Destroy();
        std::cout << "Diálogo destruido" << std::endl;
        wxString contenido =
        "MANUAL DE USO DEL CHAT\n\n"
        "1. CONTACTOS\n"
        "   - Los contactos disponibles aparecen en la lista de la derecha\n"
        "   - Los estados se muestran con los siguientes símbolos:\n"
        "     [+] Usuario Activo\n"
        "     [!] Usuario Ocupado\n"
        "     [~] Usuario INACTIVO\n"
        "     [-] Usuario Desconectado\n\n"
        "2. CHAT\n"
        "   - Seleccione un contacto para iniciar un chat\n"
        "   - Escriba su mensaje y presione el botón de la flecha para enviar\n"
        "   - Use el chat general para mensajes públicos\n\n"
        "3. ESTADO\n"
        "   - Puede cambiar su estado usando el selector en la parte superior derecha\n"
        "   - Sus mensajes no se enviarán si su estado es OCUPADO\n\n"
        "4. INFORMACIÓN\n"
        "   - Presione el botón 'Info' para ver detalles de un contacto seleccionado\n"
        "   - Presione el botón de actualizar para refrescar la lista de contactos";

    wxMessageBox(contenido, "Manual de Ayuda", wxOK | wxICON_INFORMATION);
    }
    

void VistaChat::obtenerListaUsuarios() {
    try {
        std::vector<uint8_t> solicitud = crearSolicitudListaUsuarios();
        conexion->write(red::buffer(solicitud));
    } catch (const std::exception& e) {
        wxMessageBox("Error al solicitar lista de usuarios: " + std::string(e.what()),
                   "Error", wxOK | wxICON_ERROR);
    }
}

void VistaChat::obtenerHistorialChat() {
    if (contactoActivo.empty()) return;
    
    try {
        std::vector<uint8_t> solicitud = crearSolicitudHistorial(contactoActivo);
        conexion->write(red::buffer(solicitud));
    } catch (const std::exception& e) {
        wxMessageBox("Error al solicitar historial de chat: " + std::string(e.what()),
                   "Error", wxOK | wxICON_ERROR);
    }
}

bool VistaChat::puedeEnviarMensajes() const {
    return estadoActualUsuario == EstadoUsuario::ACTIVO || estadoActualUsuario == EstadoUsuario::INACTIVO;
}

bool VistaChat::estaConectado() {
    if (!conexion) return false;
    
    try {
        return conexion->is_open() && conexion->next_layer().is_open();
    } catch (...) {
        return false;
    }
}

void VistaChat::alEnviarMensaje(wxCommandEvent&) {
    if (contactoActivo.empty()) {
        wxMessageBox("Por favor seleccione un contacto primero", "Aviso", wxOK | wxICON_INFORMATION);
        return;
    }
    
    if (!puedeEnviarMensajes()) {
        wxMessageBox("No puede enviar mensajes mientras está OCUPADO o DESCONECTADO",
                   "Aviso", wxOK | wxICON_WARNING);
        return;
    }

    if (!verificarConexion()) {
        return;  
    }

    std::string textoMensaje = campoEntradaMensaje->GetValue().ToStdString();
    if (textoMensaje.empty()) return;

    try {
        std::vector<uint8_t> datosMensaje = crearSolicitudEnvioMensaje(contactoActivo, textoMensaje);
        if (datosMensaje.empty()) return; 

        try {
            conexion->write(red::buffer(datosMensaje));
            if (estadoActualUsuario == EstadoUsuario::INACTIVO) {
                estadoActualUsuario = EstadoUsuario::ACTIVO;
                actualizarVistaEstado();
                selectorEstado->SetSelection(0); 
            }
            campoEntradaMensaje->Clear();
        } catch (const std::exception& e) {
            if (reconectar()) {
                try {
                    conexion->write(red::buffer(datosMensaje));
                    campoEntradaMensaje->Clear();
                    wxMessageBox("Mensaje enviado después de reconectar", "Reconexión Exitosa", wxOK | wxICON_INFORMATION);
                } catch (const std::exception& e2) {
                    wxMessageBox("No se pudo enviar el mensaje después de reconectar: " + std::string(e2.what()),
                               "Error", wxOK | wxICON_ERROR);
                }
            } else {
                wxMessageBox("Error al enviar mensaje: " + std::string(e.what()),
                           "Error", wxOK | wxICON_ERROR);
            }
        }
    } catch (const std::exception& e) {
        wxMessageBox("Error al preparar mensaje: " + std::string(e.what()),
                   "Error", wxOK | wxICON_ERROR);
    }
}

void VistaChat::iniciarEscuchaMensajes() {
    std::thread([this]() {
        try {
            while (estaEjecutando) {
                bestia::flat_buffer buffer;
                conexion->read(buffer);
                
                std::string datosStr = bestia::buffers_to_string(buffer.data());
                std::vector<uint8_t> mensaje(datosStr.begin(), datosStr.end());
                
                if (!mensaje.empty()) {
                    uint8_t tipoMensaje = mensaje[0];
                    
                    // Procesar mensaje según tipo
                    switch (tipoMensaje) {
                        case MSG_SERVIDOR_ERROR:
                            manejarMensajeError(mensaje);
                            break;
                        case MSG_SERVIDOR_LISTA_USUARIOS:
                            manejarMensajeListaUsuarios(mensaje);
                            break;
                        case MSG_SERVIDOR_INFO_USUARIO:
                            manejarMensajeInfoUsuario(mensaje);
                            break;
                        case MSG_SERVIDOR_USUARIO_CONECTADO:
                            manejarMensajeNuevoUsuario(mensaje);
                            break;
                        case MSG_SERVIDOR_CAMBIO_ESTADO:
                            manejarMensajeCambioEstado(mensaje);
                            break;
                        case MSG_SERVIDOR_NUEVO_MENSAJE:
                            manejarMensajeChat(mensaje);
                            break;
                        case MSG_SERVIDOR_HISTORIAL_CHAT:
                            manejarMensajeHistorialChat(mensaje);
                            break;
                        default:
                            
                            break;
                    }
                }
            }
        } catch (const bestia::error_code& ec) {
            if (ec == websocket::error::closed) {
                wxGetApp().CallAfter([this]() {
                    wxMessageBox("Conexión cerrada por el servidor", "Aviso", wxOK | wxICON_INFORMATION);
                    Close();
                });
            } else {
                wxGetApp().CallAfter([this, ec]() {
                    wxMessageBox("Error de conexión: " + ec.message(),
                               "Error", wxOK | wxICON_ERROR);
                    Close();
                });
            }
        } catch (const std::exception& e) {
            wxGetApp().CallAfter([this, e]() {
                wxMessageBox("Error de conexión: " + std::string(e.what()),
                           "Error", wxOK | wxICON_ERROR);
                Close();
            });
        }
    }).detach();
}

void VistaChat::alSeleccionarContacto(wxCommandEvent& evt) {
    wxString elementoSeleccionado = listaContactos->GetString(evt.GetSelection());
    wxString nombreContacto = elementoSeleccionado.AfterFirst(']').Trim(true).Trim(false);

    if (nombreContacto == "Chat General") {
        contactoActivo = "~";  
    } else {
        contactoActivo = nombreContacto.ToStdString();
    }

    wxString textoTitulo = wxString("Chat con: ") + 
                      (contactoActivo == "~" ? wxString("Chat General") : wxString(contactoActivo));
    etiquetaTituloChat->SetLabel(textoTitulo);

    panelHistorialChat->Clear();
    obtenerHistorialChat();
}
void VistaChat::alSolicitarInfoUsuario(wxCommandEvent&) {
    if (listaContactos->GetSelection() == wxNOT_FOUND) {
        wxMessageBox("Por favor seleccione un usuario primero", "Aviso", wxOK | wxICON_INFORMATION);
        return;
    }
    
    wxString elementoSeleccionado = listaContactos->GetString(listaContactos->GetSelection());
    wxString nombreContacto = elementoSeleccionado.AfterFirst(']').Trim(true).Trim(false);
    
    if (nombreContacto == "Chat General") {
        wxMessageBox("No se puede obtener información del chat general", "Aviso", wxOK | wxICON_INFORMATION);
        return;
    }
    
    std::string nombreUsuario = nombreContacto.ToStdString();
    
    try {
        std::cout << "Solicitando información para usuario: " << nombreUsuario << std::endl;
        
        // Crear mensaje
        std::vector<uint8_t> mensaje = crearSolicitudInfoUsuario(nombreUsuario);
        
        std::cout << "Mensaje creado: [";
        for (uint8_t byte : mensaje) {
            std::cout << (int)byte << " ";
        }
        std::cout << "]" << std::endl;
        
        // Enviar mensaje
        if (conexion->is_open()) {
            conexion->write(red::buffer(mensaje));
            std::cout << "Mensaje enviado exitosamente" << std::endl;
        } else {
            std::cout << "Error: WebSocket no está abierto" << std::endl;
            wxMessageBox("La conexión con el servidor está cerrada", "Error", wxOK | wxICON_ERROR);
        }
    } catch (const std::exception& e) {
        std::cerr << "Excepción al solicitar información: " << e.what() << std::endl;
        wxMessageBox("Error al solicitar información: " + std::string(e.what()), "Error", wxOK | wxICON_ERROR);
    }
}
void VistaChat::alActualizarContactos(wxCommandEvent&) {
    obtenerListaUsuarios();
}


void VistaChat::alCambiarEstado(wxCommandEvent&) {
    int seleccion = selectorEstado->GetSelection();
    EstadoUsuario nuevoEstado;
    
    switch (seleccion) {
        case 0: nuevoEstado = EstadoUsuario::ACTIVO; break;
        case 1: nuevoEstado = EstadoUsuario::OCUPADO; break;
        default: nuevoEstado = EstadoUsuario::ACTIVO; break;
    }
    
    EstadoUsuario estadoAnterior = estadoActualUsuario;
    
    try {
        std::vector<uint8_t> actualizacionEstado = crearSolicitudActualizacionEstado(nuevoEstado);
        
        // Actualizar UI primero
        estadoActualUsuario = nuevoEstado;
        actualizarVistaEstado();

        // Enviar actualización de estado al servidor
        conexion->write(red::buffer(actualizacionEstado));

        // Actualizar lista de usuarios
        std::vector<uint8_t> solicitudActualizar = crearSolicitudListaUsuarios();
        conexion->write(red::buffer(solicitudActualizar));
        
        std::cout << "⏩ Estado cambiado a " << obtenerNombreEstado(nuevoEstado) << ". Notificando al servidor..." << std::endl;        
    } catch (const std::exception& e) {
        std::cerr << "Error al cambiar estado: " << e.what() << std::endl;

        // Revertir al estado anterior en caso de error
        estadoActualUsuario = estadoAnterior;
        actualizarVistaEstado();
        
        wxMessageBox("Error al cambiar estado: " + std::string(e.what()), 
                   "Error", wxOK | wxICON_ERROR);
    }
}

bool VistaChat::reconectar() {
    try {
        // Cerrar conexión actual
        conexion->close(websocket::close_code::normal);

        // Obtener información del punto final de la conexión actual
        red::io_context contextoIO;
        tcp::resolver resolvedor(contextoIO);

        std::string direccionServidor = conexion->next_layer().remote_endpoint().address().to_string();
        unsigned short puertoServidor = conexion->next_layer().remote_endpoint().port();
        
        // Resolver y conectar
        auto puntosFinal = resolvedor.resolve(direccionServidor, std::to_string(puertoServidor));
        
        tcp::socket socket(contextoIO);
        red::connect(socket, puntosFinal);
        
        // Crear nuevo WebSocket
        auto nuevaConexion = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
        nuevaConexion->set_option(websocket::stream_base::timeout::suggested(bestia::role_type::client));
        
        // Realizar handshake
        std::string anfitrion = direccionServidor;
        std::string objetivo = "/?name=" + usuarioActual;
        
        nuevaConexion->handshake(anfitrion, objetivo);

        // Reemplazar conexión antigua con la nueva
        conexion = nuevaConexion;
        
        // Actualizar datos
        obtenerListaUsuarios();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error al reconectar: " << e.what() << std::endl;
        return false;
    }
}

bool VistaChat::verificarConexion() {
    if (!estaConectado()) {
        bool reconectado = reconectar();
        if (reconectado) {
            wxMessageBox("Conexión restablecida correctamente.", 
                       "Reconexión", wxOK | wxICON_INFORMATION);
            return true;
        } else {
            wxMessageBox("No se pudo restablecer la conexión con el servidor.", 
                       "Error de Conexión", wxOK | wxICON_ERROR);
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> VistaChat::crearSolicitudListaUsuarios() {
    return {MSG_CLIENTE_SOLICITAR_USUARIOS};
}

std::vector<uint8_t> VistaChat::crearSolicitudInfoUsuario(const std::string& nombreUsuario) {
    std::vector<uint8_t> mensaje;
    
    mensaje.push_back(MSG_CLIENTE_OBTENER_INFO_USUARIO);
    
    mensaje.push_back(static_cast<uint8_t>(nombreUsuario.size()));
    
    for (char c : nombreUsuario) {
        mensaje.push_back(static_cast<uint8_t>(c));
    }
    
    return mensaje;
}

std::vector<uint8_t> VistaChat::crearSolicitudActualizacionEstado(EstadoUsuario nuevoEstado) {
    std::vector<uint8_t> mensaje = {
        MSG_CLIENTE_ACTUALIZAR_ESTADO, 
        static_cast<uint8_t>(usuarioActual.size())
    };
    mensaje.insert(mensaje.end(), usuarioActual.begin(), usuarioActual.end());
    mensaje.push_back(static_cast<uint8_t>(nuevoEstado));
    return mensaje;
}

std::vector<uint8_t> VistaChat::crearSolicitudEnvioMensaje(const std::string& destinatario, const std::string& mensaje) {
    // Verificar límite de longitud del mensaje
    if (mensaje.size() > 255) {
        wxMessageBox("El mensaje es demasiado largo (máximo 255 caracteres)", 
                    "Aviso", wxOK | wxICON_WARNING);
        return {};
    }
    
    try {
        // Construir paquete de protocolo del mensaje
        std::vector<uint8_t> datos = {
            MSG_CLIENTE_ENVIAR_MENSAJE, 
            static_cast<uint8_t>(destinatario.size())
        };
        datos.insert(datos.end(), destinatario.begin(), destinatario.end());
        datos.push_back(static_cast<uint8_t>(mensaje.size()));
        datos.insert(datos.end(), mensaje.begin(), mensaje.end());
        return datos;
    } catch (const std::exception& e) {
        wxMessageBox("Error al crear mensaje: " + std::string(e.what()), 
                   "Error", wxOK | wxICON_ERROR);
        return {};
    }
}

std::vector<uint8_t> VistaChat::crearSolicitudHistorial(const std::string& contactoChat) {
    std::vector<uint8_t> mensaje = {MSG_CLIENTE_SOLICITAR_HISTORIAL, static_cast<uint8_t>(contactoChat.size())};
    mensaje.insert(mensaje.end(), contactoChat.begin(), contactoChat.end());
    return mensaje;
}
