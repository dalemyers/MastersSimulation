
packet DataPacket
{
    int uuid;
    char busid;
    string debugMessage;
    bool serverClose;
    
    
    ///Air Quality Measurements Here
    
    short temperature;
    short co2concentration;
    short no3concentration;
    short o3concentration;
    short pm10concentration;
    short pm25concentration;

    
}
