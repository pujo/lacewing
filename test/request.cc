
#include <Lacewing.h>

void onGet(Lacewing::Webserver &Webserver, Lacewing::Webserver::Request &Request)
{
    Request << "<p>Hello world from " << Lacewing::Version() << "</p>";
    
    Request << "<p>Headers used in this request:</p><p>";
   
    for (struct Lacewing::Webserver::Request::Header * Header
                = Request.FirstHeader (); Header; Header = Header->Next ())
    {
        Request << Header->Name () << ": " << Header->Value () << "<br />";
    }
    
    Request << "</p><p>POST items:</p><p>";
   
    for (struct Lacewing::Webserver::Request::Parameter * Item
                = Request.POST (); Item; Item = Item->Next ())
    {
        Request << Item->Name () << ": " << Item->Value () << "<br />";
    }
    
    Request << "</p><p>GET items:</p><p>";
   
    for (struct Lacewing::Webserver::Request::Parameter * Item
                = Request.GET (); Item; Item = Item->Next ())
    {
        Request << Item->Name () << ": " << Item->Value () << "<br />";
    }
    
    Request << "</p>";
}

int main(int argc, char * argv[])
{
    Lacewing::EventPump EventPump;
    Lacewing::Webserver Webserver(EventPump);

    Webserver.onGet (onGet);
    Webserver.onPost (onGet);

    Webserver.Host (8020);    
    
    EventPump.StartEventLoop();
    
    return 0;
}

