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
            // Cierra la aplicación
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
    
    directorioContactos.insert({"~", Contacto("Chat General", EstadoUsuario::ACTIVO)});
    
    wxPanel* panelPrincipal = new wxPanel(this);
    panelPrincipal->SetBackgroundColour(wxColour(32, 32, 32)); // Fondo oscuro para el panel principal
    
    wxBoxSizer* diseñoPrincipal = new wxBoxSizer(wxHORIZONTAL);


    wxBoxSizer* panelIzquierdo = new wxBoxSizer(wxVERTICAL);

    // Título del chat con estilo
    etiquetaTituloChat = new wxStaticText(panelPrincipal, wxID_ANY, "Chat con: Chat General");
    etiquetaTituloChat->SetForegroundColour(wxColour(200, 200, 200)); // Texto claro para contraste
    wxFont fuenteTitulo = etiquetaTituloChat->GetFont();
    fuenteTitulo.SetPointSize(fuenteTitulo.GetPointSize() + 2);
    fuenteTitulo.SetWeight(wxFONTWEIGHT_BOLD);
    etiquetaTituloChat->SetFont(fuenteTitulo);
    panelIzquierdo->Add(etiquetaTituloChat, 0, wxALL, 10);

    panelHistorialChat = new wxTextCtrl(panelPrincipal, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                     wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    panelHistorialChat->SetBackgroundColour(wxColour(45, 45, 45)); // Fondo un poco más claro que el principal
    panelHistorialChat->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro para contraste
    panelIzquierdo->Add(panelHistorialChat, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

    wxBoxSizer* diseñoEntradaMensaje = new wxBoxSizer(wxHORIZONTAL);
    campoEntradaMensaje = new wxTextCtrl(panelPrincipal, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 
                                      wxTE_PROCESS_ENTER);
    campoEntradaMensaje->SetBackgroundColour(wxColour(60, 60, 60)); // Fondo más claro para el campo de texto
    campoEntradaMensaje->SetForegroundColour(wxColour(220, 220, 220)); // Texto claro
    diseñoEntradaMensaje->Add(campoEntradaMensaje, 1, wxALL, 10);

    botonEnviar = new wxBitmapButton(panelPrincipal, wxID_ANY, 
        wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_BUTTON));
    botonEnviar->SetToolTip("Enviar mensaje");
    botonEnviar->SetBackgroundColour(wxColour(70, 130, 180)); // Azul acero
    diseñoEntradaMensaje->Add(botonEnviar, 0, wxTOP | wxRIGHT | wxBOTTOM, 10);
    
    panelIzquierdo->Add(diseñoEntradaMensaje, 0, wxEXPAND);

    wxBoxSizer* panelDerecho = new wxBoxSizer(wxVERTICAL);

    // Título de la sección de contactos
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

    // Visualización de estado con colores
    etiquetaEstado = new wxStaticText(panelPrincipal, wxID_ANY, "Estado actual: ACTIVO");
    etiquetaEstado->SetForegroundColour(wxColour(50, 205, 50));
    panelDerecho->Add(etiquetaEstado, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 10);

    wxStaticLine* lineaSeparadora = new wxStaticLine(panelPrincipal, wxID_ANY, wxDefaultPosition, 
                                                wxDefaultSize, wxLI_HORIZONTAL);
    panelDerecho->Add(lineaSeparadora, 0, wxEXPAND | wxALL, 10);

    wxBoxSizer* contactosHeader = new wxBoxSizer(wxHORIZONTAL);

    // Add the contacts label
    wxStaticText* etiquetaTituloContactos = new wxStaticText(panelPrincipal, wxID_ANY, "Contactos:");
    etiquetaTituloContactos->SetForegroundColour(wxColour(200, 200, 200));
    etiquetaTituloContactos->SetFont(fuenteTituloSeccion);
    contactosHeader->Add(etiquetaTituloContactos, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

    // Add the refresh button
    botonActualizar = new wxBitmapButton(panelPrincipal, wxID_ANY, 
        wxArtProvider::GetBitmap(wxART_REFRESH, wxART_BUTTON, wxSize(16, 16)));
    botonActualizar->SetToolTip("Actualizar lista de contactos");
    botonActualizar->SetBackgroundColour(wxColour(60, 60, 60));
    contactosHeader->Add(botonActualizar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    // Add the header to the panel
    panelDerecho->Add(contactosHeader, 0, wxEXPAND | wxTOP, 10);

    // Create and configure the contacts list
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
    botonCerrarSesion->SetBackgroundColour(wxColour(169, 68, 66)); // Rojo oscuro
    botonCerrarSesion->SetForegroundColour(wxColour(255, 255, 255)); // Texto blanco
    diseñoBotonesContacto->Add(botonCerrarSesion, 1, wxALL, 5);
    panelDerecho->Add(diseñoBotonesContacto, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 10);

    diseñoPrincipal->Add(panelIzquierdo, 2, wxEXPAND | wxALL, 10);
    
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
            colorEstado = wxColour(50, 205, 50);  // Verde claro para modo oscuro
            break;
        case EstadoUsuario::OCUPADO:
            textoEstado = "OCUPADO";
            colorEstado = wxColour(255, 99, 71);  // Rojo tomate para modo oscuro
            break;
        case EstadoUsuario::INACTIVO:
            textoEstado = "INACTIVO";
            colorEstado = wxColour(255, 215, 0);  // Oro para modo oscuro
            break;
        case EstadoUsuario::DESCONECTADO:
            textoEstado = "DESCONECTADO";
            colorEstado = wxColour(169, 169, 169);  // Gris oscuro para modo oscuro
            break;
    }
    
    etiquetaEstado->SetLabel("Estado actual: " + textoEstado);
    etiquetaEstado->SetForegroundColour(colorEstado);
    
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