#pragma once
//
// rt_xml.h - winziger, abhaengigkeitsfreier XML-Leser fuer den Replayer.
//
// Reicht genau fuer das, was JUCEs XmlElement::writeTo erzeugt (und damit fuer
// das .retrotrax-Format): Elemente, Attribute (doppelte Anfuehrungszeichen),
// Kind-Elemente, Text-Inhalt (fuer die Base64-Sample-Bloecke), self-closing
// Tags, die <?xml ...?>-Deklaration und <!-- Kommentare -->. KEIN voller
// XML-Parser - nur dieser klar umrissene Ausschnitt.

#include <cctype>
#include <map>
#include <string>
#include <vector>

namespace rtxml
{
    struct Node
    {
        std::string tag;
        std::map<std::string, std::string> attrs;
        std::vector<Node> children;
        std::string text; // direkter Text-Inhalt (zusammengefasst)

        const Node* child (const std::string& name) const
        {
            for (const auto& c : children)
                if (c.tag == name) return &c;
            return nullptr;
        }
        bool has (const std::string& a) const { return attrs.find (a) != attrs.end(); }
        std::string attr (const std::string& a, const std::string& def = "") const
        {
            auto it = attrs.find (a);
            return it != attrs.end() ? it->second : def;
        }
        int    attrInt    (const std::string& a, int d = 0)    const { return has (a) ? std::stoi (attrs.at (a)) : d; }
        double attrDouble (const std::string& a, double d = 0) const { return has (a) ? std::stod (attrs.at (a)) : d; }
    };

    namespace detail
    {
        inline std::string decodeEntities (const std::string& s)
        {
            std::string out;
            out.reserve (s.size());
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] != '&') { out += s[i]; continue; }
                size_t semi = s.find (';', i);
                if (semi == std::string::npos) { out += s[i]; continue; }
                std::string ent = s.substr (i + 1, semi - i - 1);
                if      (ent == "amp")  out += '&';
                else if (ent == "lt")   out += '<';
                else if (ent == "gt")   out += '>';
                else if (ent == "quot") out += '"';
                else if (ent == "apos") out += '\'';
                else if (! ent.empty() && ent[0] == '#')
                {
                    int code = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                               ? (int) std::stol (ent.substr (2), nullptr, 16)
                               : std::stoi (ent.substr (1));
                    out += (char) code; // reicht fuer ASCII/Latin-1-Inhalte
                }
                else { out += s.substr (i, semi - i + 1); }
                i = semi;
            }
            return out;
        }

        inline void skipWs (const std::string& s, size_t& p)
        {
            while (p < s.size() && std::isspace ((unsigned char) s[p])) ++p;
        }

        // Liest ein Element ab s[p]=='<' (kein <? oder <!). Gibt Node zurueck.
        inline Node parseElement (const std::string& s, size_t& p)
        {
            Node n;
            ++p; // '<'
            // Tag-Name
            while (p < s.size() && ! std::isspace ((unsigned char) s[p]) && s[p] != '>' && s[p] != '/')
                n.tag += s[p++];

            // Attribute
            while (true)
            {
                skipWs (s, p);
                if (p >= s.size() || s[p] == '>' || s[p] == '/') break;
                std::string key;
                while (p < s.size() && s[p] != '=' && ! std::isspace ((unsigned char) s[p]) && s[p] != '>')
                    key += s[p++];
                skipWs (s, p);
                std::string val;
                if (p < s.size() && s[p] == '=')
                {
                    ++p; skipWs (s, p);
                    if (p < s.size() && (s[p] == '"' || s[p] == '\''))
                    {
                        char q = s[p++];
                        std::string raw;
                        while (p < s.size() && s[p] != q) raw += s[p++];
                        if (p < s.size()) ++p; // schliessendes Quote
                        val = decodeEntities (raw);
                    }
                }
                if (! key.empty()) n.attrs[key] = val;
            }

            if (p < s.size() && s[p] == '/') { p += 2; return n; } // self-closing "/>"
            if (p < s.size() && s[p] == '>') ++p;

            // Inhalt bis </tag>
            std::string textBuf;
            while (p < s.size())
            {
                if (s[p] == '<')
                {
                    if (s.compare (p, 4, "<!--") == 0)        // Kommentar
                    {
                        size_t e = s.find ("-->", p);
                        p = (e == std::string::npos) ? s.size() : e + 3;
                        continue;
                    }
                    if (s.compare (p, 2, "</") == 0)          // End-Tag
                    {
                        p += 2;
                        while (p < s.size() && s[p] != '>') ++p;
                        if (p < s.size()) ++p;
                        break;
                    }
                    n.children.push_back (parseElement (s, p)); // Kind
                }
                else
                {
                    textBuf += s[p++];
                }
            }
            n.text = decodeEntities (textBuf);
            return n;
        }
    }

    // Parst ein komplettes XML-Dokument und gibt das Wurzel-Element zurueck.
    inline Node parse (const std::string& s)
    {
        size_t p = 0;
        while (p < s.size())
        {
            detail::skipWs (s, p);
            if (p + 1 >= s.size()) break;
            if (s.compare (p, 2, "<?") == 0)        { size_t e = s.find ("?>", p);  p = (e == std::string::npos) ? s.size() : e + 2; continue; }
            if (s.compare (p, 4, "<!--") == 0)      { size_t e = s.find ("-->", p);  p = (e == std::string::npos) ? s.size() : e + 3; continue; }
            if (s.compare (p, 2, "<!") == 0)        { size_t e = s.find ('>', p);    p = (e == std::string::npos) ? s.size() : e + 1; continue; }
            if (s[p] == '<')                        return detail::parseElement (s, p);
            ++p;
        }
        return {};
    }
}
