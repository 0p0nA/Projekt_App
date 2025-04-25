#include <wx/wx.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
#include <iostream>

using json = nlohmann::json;//definiuje alias json

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output){ //funkcja callback do przetwarzania odebranych danych
    size_t totalSize = size*nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

json fetchData(const std::string& url){ //funkcja pobierania danych
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl= curl_easy_init();
    if(curl){
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if(res==CURLE_OK){
            try{
                auto parsed = json::parse(readBuffer);
                if(parsed.empty()) wxMessageBox("Pobrano, ale dane puste", "Debug");
                return parsed;
            }catch (...){
                wxMessageBox("Błąd parsowania JSON", "Debug");
            }
        }        
    }
    return nullptr;
}

void SaveJsonToFile(const json& data, const std::string& filename){ //zapisywanie danych do pliku json
    std::ofstream file(filename);
    if(file.is_open()){
        file<<data.dump(4);
    }
}


void DownloadAllSensorsAndSave(const json& stations){ //pobieranie danych o stanowiskach p.
    json allSensors;

    for(const auto& station:stations){
        int stationId = station["id"];
        auto sensors = fetchData("https://api.gios.gov.pl/pjp-api/rest/station/sensors/"+std::to_string(stationId));
        if(sensors.is_array()){
            allSensors[std::to_string(stationId)] = sensors;
        }
    }
    std::ofstream file("sensors.json");
    if(file.is_open()) {
        file << allSensors.dump(2);
        file.close();
    } 
    else{
        wxMessageBox("Nie udalo sie zapisac sensors.json", "Blad");
    }
}

void DownloadAllSensorDataAndSave(const std::string& sensorsFilePath, const std::string& outputFilePath){ //pobieranie danych pomiarowych
    std::ifstream sensorsFile(sensorsFilePath);
    if(!sensorsFile.is_open()){
        wxMessageBox("Nie udało się otworzyć pliku sensors.json", "Błąd",wxICON_ERROR);
        return;
    }
    json sensorsData;
    try{
        sensorsFile>>sensorsData;
    }catch (...){
        wxMessageBox("Nieprawidłowy format pliku sensors.json", "Błąd", wxICON_ERROR);
        return;
    }
    json allData = json::array();

    for(auto& [stationIdStr, sensorList] : sensorsData.items()){
        int stationId = std::stoi(stationIdStr);
        for(const auto& sensor:sensorList){
            int sensorId = sensor["id"];
            json data = fetchData("https://api.gios.gov.pl/pjp-api/rest/data/getData/"+std::to_string(sensorId));
    
            if(data.is_object()){
                data["sensorId"] = sensorId;
                data["stationId"] = stationId;
                allData.push_back(data);
            }
        }
    }
    
    std::ofstream outFile(outputFilePath);
    if (outFile.is_open()){
        outFile<<allData.dump(4);
    }
}

class MyApp:public wxApp{ //klasa aplikacji
public:
    virtual bool OnInit();

private:
    wxLocale locale;
};

class MyFrame:public wxFrame{ //klasa - okno główne
public:
    MyFrame();

private:
    wxListBox* stationList;
    wxListBox* sensorList;
    wxTextCtrl* sensorData;
    wxStaticText* stationInfoText;
    wxStaticText* sensorInfo;
    wxStaticText* airQualityText;

    wxChoice* sortChoice;

    std::vector<json> stationsData;
    std::vector<json> sensorsData;
    std::vector<std::pair<std::string, double>> currentSensorData;
    std::string currentParamName;

    void OnStationSelected(wxCommandEvent& event);
    void OnSensorSelected(wxCommandEvent& event);
    void OnSortChanged(wxCommandEvent& event);
    void LoadStations();
    void LoadSensors(int stationId);
    void LoadSensorData(int sensorId);
    void SortStations();
    void OnShowPlot(wxCommandEvent& event);
};

class PlotFrame:public wxFrame{ //klasa okno poboczne - wykres
    public:
        PlotFrame(wxWindow* parent, const std::vector<std::pair<std::string, double>>& data, const std::string& paramName);
    private:
        std::vector<std::pair<std::string, double>> plotData;
        std::string yAxisLabel;
        wxPanel* panel;
        void OnPaint(wxPaintEvent& event);
    };
    
IMPLEMENT_APP(MyApp)

bool MyApp::OnInit(){ // inicjalizacja aplikacji
    locale.Init(wxLANGUAGE_POLISH);
    wxLocale::AddCatalogLookupPathPrefix(".");
    locale.AddCatalog("wxstd");
    wxConvCurrent= &wxConvUTF8;

    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame() //konstruktor MyFrame
    :wxFrame(NULL, wxID_ANY, "Stacje pomiarowe", wxDefaultPosition, wxSize(1400, 900)){
    wxPanel* panel = new wxPanel(this, -1);
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* sortSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* sortLabel = new wxStaticText(panel, wxID_ANY, "Sortuj:");
    sortChoice = new wxChoice(panel, wxID_ANY);

    sortChoice->Append("wg nazwy");
    sortChoice->Append("wg ID");
    sortChoice->SetSelection(1);
    sortSizer->Add(sortLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    sortSizer->Add(sortChoice, 0, wxALIGN_CENTER_VERTICAL);

    leftSizer->Add(sortSizer, 0, wxALL, 10);
    stationList = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 480));
    leftSizer->Add(stationList, 0, wxEXPAND | wxALL, 10);
    stationInfoText = new wxStaticText(panel, wxID_ANY,"", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    stationInfoText->Wrap(300);
    leftSizer->Add(stationInfoText, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* middleSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* sensorLabel = new wxStaticText(panel,wxID_ANY, "Stanowiska pomiarowe:");
    sensorList = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 140));
    middleSizer->Add(sensorLabel, 0,wxALL, 10);
    middleSizer->Add(sensorList, 0, wxEXPAND | wxALL, 10);
    sensorInfo = new wxStaticText(panel, wxID_ANY,"", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    sensorInfo->Wrap(300);
    middleSizer->Add(sensorInfo, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    middleSizer->AddSpacer(10);
    airQualityText = new wxStaticText(panel, wxID_ANY,"", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    airQualityText->Wrap(300);
    middleSizer->Add(airQualityText, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* sensorDataLabel = new wxStaticText(panel, wxID_ANY, "Dane pomiarowe:");
    sensorData = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 600), wxTE_MULTILINE | wxTE_READONLY);
    rightSizer->Add(sensorDataLabel, 0, wxALL, 10);
    rightSizer->Add(sensorData, 0, wxEXPAND | wxALL, 10);
    wxButton* showPlotButton = new wxButton(panel, wxID_ANY, wxString::FromUTF8("Pokaż wykres"));
    showPlotButton->Bind(wxEVT_BUTTON, &MyFrame::OnShowPlot, this);
    rightSizer->Add(showPlotButton, 0, wxALL | wxALIGN_RIGHT, 10);

    mainSizer->Add(leftSizer, 1, wxEXPAND);
    mainSizer->Add(middleSizer, 1, wxEXPAND);
    mainSizer->Add(rightSizer, 1, wxEXPAND);

    panel->SetSizer(mainSizer);
    stationList->Bind(wxEVT_LISTBOX, &MyFrame::OnStationSelected, this);
    sensorList->Bind(wxEVT_LISTBOX, &MyFrame::OnSensorSelected, this);
    sortChoice->Bind(wxEVT_CHOICE, &MyFrame::OnSortChanged, this);

    LoadStations();
}

//konstruktor PlotFrame
PlotFrame::PlotFrame(wxWindow* parent, const std::vector<std::pair<std::string, double>>& data, const std::string& paramName)
    : wxFrame(parent, wxID_ANY, "Wykres danych",wxDefaultPosition, wxSize(wxGetDisplaySize().GetWidth()*2/3, wxGetDisplaySize().GetHeight()*2/3),
    wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX)), 
    plotData(data.rbegin(), data.rend()),
    yAxisLabel(paramName){    
    panel = new wxPanel(this);
    panel->SetBackgroundColour(*wxWHITE);
    panel->Bind(wxEVT_PAINT, &PlotFrame::OnPaint, this);
}

void MyFrame::LoadStations(){ //ładowanie stacji pomiarowych
    json data;
    std::ifstream file("stations.json");
    if(file.is_open()){
        try{
            file>>data;
        }catch (...){
            wxMessageBox("Nie udało się wczytać danych z pliku stations.json.", "Błąd", wxICON_ERROR);
            return;
        }
    }

    stationsData.clear();
    stationList->Clear();
    for(const auto& station:data){
        stationsData.push_back(station);
    }
    SortStations();

    std::thread([this]() {
        json onlineData = fetchData("https://api.gios.gov.pl/pjp-api/rest/station/findAll");
        if(onlineData.is_array()){
            SaveJsonToFile(onlineData, "stations.json");
            DownloadAllSensorsAndSave(onlineData);
            DownloadAllSensorDataAndSave("sensors.json", "data.json");
            this->CallAfter([this, onlineData]() {
                stationsData.clear();
                stationList->Clear();
                for(const auto& station:onlineData){
                    stationsData.push_back(station);
                }
                SortStations();
            });
        }
    }).detach();
}

void MyFrame::SortStations(){ //sortowanie  stacji
    int sortType = sortChoice->GetSelection();
    std::sort(stationsData.begin(), stationsData.end(), [sortType](const json& a, const json& b){
        if(sortType==0) return a["stationName"].get<std::string>()<b["stationName"].get<std::string>();
        else return a["id"].get<int>() < b["id"].get<int>();
    });

    stationList->Clear();
    for(const auto& station : stationsData){
        std::ostringstream entry;
        entry<<station["stationName"].get<std::string>()<<" (ID: "<<station["id"].get<int>()<<")";
        stationList->Append(wxString::FromUTF8(entry.str().c_str()));
    }
}

void MyFrame::OnSortChanged(wxCommandEvent& event){ //handler zdarzenia sortowania
    SortStations();
}

void MyFrame::OnStationSelected(wxCommandEvent& event){ //zaladowanie informacji o stacji
    int index = event.GetSelection();
    if (index>=0 && index<stationsData.size()){
        int id = stationsData[index]["id"];
        LoadSensors(id);
        const auto& station = stationsData[index];
        std::ostringstream info;
        info <<"=== Informacje o stacji pomiarowej ===\n";        
        info <<"ID stacji: " <<station["id"] << "\n"
             <<"Nazwa stacji: " <<station["stationName"] << "\n"
             <<"Szerokość geograficzna: " <<station["gegrLat"] << "\n"
             <<"Długość geograficzna: " <<station["gegrLon"] << "\n";

        if (station.contains("city")) {
            const auto& city = station["city"];
            info<< "ID lokalizacji: " <<city["id"] << "\n"
                << "Miejscowość: " <<city["name"] << "\n";

            if (city.contains("commune")) {
                const auto& commune = city["commune"];
                info << "Gmina: " <<commune["communeName"] << "\n"
                     << "Powiat: " <<commune["districtName"] << "\n"
                     << "Województwo: " <<commune["provinceName"] << "\n";
            }
        }
        info<<"Ulica: "<<(station["addressStreet"].is_null()?"brak danych" : station["addressStreet"].get<std::string>())<<"\n";
        stationInfoText->SetLabel(wxString::FromUTF8(info.str().c_str()));
    }
}

void MyFrame::LoadSensors(int stationId){ // ładuje dane stanowisk pomiarowych
    json sensors;
    auto data = fetchData("https://api.gios.gov.pl/pjp-api/rest/station/sensors/"+std::to_string(stationId));

    if(!data.is_array()){
        std::ifstream file("sensors.json");
        if(file.is_open()){
            try{
                json allSensors;
                file>>allSensors;
                sensors = allSensors[std::to_string(stationId)];
            }catch (...){
                wxMessageBox("Blad odczytu sensors.json", "Blad");
                return;
            }
        } 
        else{
            wxMessageBox("Brak internetu i brak pliku sensors.json", "Błąd");
            return;
        }
    } 
    else{
        sensors = data;
    }
    sensorsData.clear();
    sensorList->Clear();

    for(const auto& sensor:sensors){
        std::string paramName = sensor["param"]["paramName"];
        sensorList->Append(wxString::FromUTF8(paramName.c_str()));
        sensorsData.push_back(sensor);
    }
}


void MyFrame::OnSensorSelected(wxCommandEvent& event){ //obsługa zdarzenia wyboru danego sensora
    int index = event.GetSelection();
    const auto& param = sensorsData[index]["param"];
    currentParamName = param["paramName"];
    if(index>=0 && index<sensorsData.size()){
        const auto& sensor = sensorsData[index];
        int sensorId = sensor["id"];
        LoadSensorData(sensorId);

        std::ostringstream info;
        info<< "ID stanowiska: " <<sensor["id"] <<"\n";
        info<< "ID stacji: " <<sensor["stationId"] <<"\n";

        if(sensor.contains("param")){
            const auto& param = sensor["param"];
            info <<"  Nazwa parametru: " <<param["paramName"] <<"\n";
            info <<"  Symbol parametru: " <<param["paramFormula"] <<"\n";
            info <<"  Kod parametru: " <<param["paramCode"] <<"\n";
            info <<"  ID parametru: " <<param["idParam"] <<"\n";
        }

        int stationId = sensor["stationId"];
        auto indexData = fetchData("https://api.gios.gov.pl/pjp-api/rest/aqindex/getIndex/"+std::to_string(stationId));

        std::ostringstream indexText;

        if(indexData.contains("stIndexLevel")){
            indexText <<"ID stacji: " <<indexData["id"] << "\n";
            indexText <<"Data obliczenia: " <<indexData["stCalcDate"] << "\n";
            indexText <<"Najgorszy poziom indeksu:\n";
            indexText <<"  Poziom: " <<indexData["stIndexLevel"]["id"] << "\n";
            indexText <<"  Opis: " <<indexData["stIndexLevel"]["indexLevelName"] << "\n";
            indexText <<"Źródło danych: " <<indexData["stSourceDataDate"] << "\n";
        } else{
            indexText <<"Brak danych indeksu.\n";
        }

        std::ostringstream fullInfo;
        fullInfo <<"=== Informacje o stanowisku pomiarowym ===\n";
        fullInfo <<info.str();
        fullInfo <<"\n=== Indeks jakości powietrza ===\n";
        fullInfo <<indexText.str();
        airQualityText->SetLabel(wxString::FromUTF8(fullInfo.str().c_str()));
        Layout();
    }
}

void MyFrame::LoadSensorData(int sensorId){ //pobieranie i wyświetlanie danych pomiarowych
    json data = fetchData("https://api.gios.gov.pl/pjp-api/rest/data/getData/"+std::to_string(sensorId));
    if(!data.is_object() || !data.contains("values")){
        std::ifstream file("data.json");
        if(file.is_open()){
            try{
                json allData;
                file>>allData;
                for(const auto& item : allData){
                    if(item.contains("sensorId") && item["sensorId"]==sensorId){
                        data = item;
                        break;
                    }
                }
            }catch (...){
                sensorData->SetValue("Błąd odczytu pliku data.json.");
                return;
            }
        }

        if(!data.is_object() || !data.contains("values")){
            sensorData->SetValue("Brak danych pomiarowych.");
            return;
        }
    }

    std::ostringstream sensorDataText;
    bool hasData = false;
    currentSensorData.clear();
    for(const auto& item:data["values"]){
        if(!item["value"].is_null()){
            std::string date = item["date"].get<std::string>();
            double value = item["value"].get<double>();
            sensorDataText<< "Data: "<< date<< " | Wartość: "<<value<<"\n";
            currentSensorData.emplace_back(date, value);
            hasData = true;
        }
    }

    if(!hasData){
        sensorData->SetValue("Brak danych pomiarowych.");
    } 
    else{
        sensorData->SetValue(wxString::FromUTF8(sensorDataText.str().c_str()));
    }
}


void MyFrame::OnShowPlot(wxCommandEvent& event){//handler zdarzenia klknięcia na "Pokaż wykres"
    if(currentSensorData.empty()){
        wxMessageBox("Brak danych pomiarowych dla wykresu.", "Błąd", wxICON_ERROR);
        return;
    }
    PlotFrame* plotFrame = new PlotFrame(this, currentSensorData, currentParamName+" [mg/m³]");
    plotFrame->Show(true);
}


void PlotFrame::OnPaint(wxPaintEvent& event){//rysowanie wykresu
    wxPaintDC dc(panel);
    dc.Clear();
    if(plotData.empty()){
        dc.DrawText("Brak danych do wyświetlenia.", 10, 10);
        return;
    }
    int w, h;
    panel->GetClientSize(&w, &h);
    auto [minIt, maxIt]=std::minmax_element(plotData.begin(), plotData.end(),
        [](auto& a, auto& b) {return a.second < b.second;});

    double minY = minIt->second;
    double maxY = maxIt->second;
    int margin = 50;
    int graphWidth = w-2*margin;
    int graphHeight = h-2*margin;
    dc.DrawLine(margin, margin, margin, h-margin); 
    dc.DrawLine(margin, h-margin, w-margin, h-margin);

    wxFont font(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial");
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(0, 0, 0));

    int labelStep = std::max(1, (int)(plotData.size()/10));
    for(size_t i = 0; i<plotData.size(); i+=labelStep){
        double x = margin+i*(graphWidth/double(plotData.size()-1));
        std::string timestamp = plotData[i].first;
        std::string label = timestamp.substr(5, 5)+"\n"+timestamp.substr(11, 5);
        dc.DrawText(wxString(label), x-30, h-margin+5);        
    }

    size_t count = plotData.size();
    if(count<2) return;
    for(size_t i = 1; i<count; ++i){
        double x0 = margin+(i-1)*(graphWidth/double(count-1));
        double y0 = h-margin-((plotData[i-1].second-minY)/(maxY-minY))*graphHeight;
        double x1 = margin + i*(graphWidth/double(count-1));
        double y1 = h-margin-((plotData[i].second-minY)/(maxY-minY))*graphHeight;
        dc.DrawLine(x0, y0, x1, y1);
    }
}
