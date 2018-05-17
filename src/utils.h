#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <functional>
#include <sys/stat.h>
//#include <unistd.h>
//#include <dirent.h>
#include <cstring>
#include <deque>
#include <algorithm>
#include <ctime>
#include <set>

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#ifdef _WIN32
#define ftell_x _ftelli64
#define fseek_x _fseeki64
#else
#define ftell_x ftell
#define fseek_x fseek
#endif

template<class C>
std::vector<std::basic_string<C> > split(const std::basic_string<C> &s,
    const std::string &delim = " ", unsigned limit = 0)
{
    std::vector<std::basic_string<C> > args;
    auto l = delim.length();
    if (l == 0)
        return args;
    unsigned pos = 0;
    bool crlf = (delim.size() == 1 && delim[0] == 10);
    while (true)
    {
        auto newpos = s.find(delim, pos);
        if ((limit && args.size() == limit) || newpos == std::string::npos)
        {
            args.push_back(s.substr(pos));
            break;
        }
        args.push_back(s.substr(pos, newpos - pos));
        pos = newpos + l;
        if (crlf && pos < s.length() && s[pos] == 13)
            pos++;
    }

    return args;
}

template<class C>
std::vector<std::basic_string<C> > split(const C *s, const std::string &delim = " ",
    unsigned limit = 0)
{
    return split(std::basic_string<C>(s), delim, limit);
}

bool fileExists(const std::string &name);

template<class CONTAINER> void listFiles(char *dirName, CONTAINER &rc, int &strSize)
{
    DIR *dir;
    struct dirent *ent;

    struct stat ss;
    stat(dirName, &ss);
    if ((ss.st_mode & S_IFDIR) == 0)
    {
        rc.emplace_back(dirName);
        strSize += strlen(dirName);
        return;
    }

    if ((dir = opendir(dirName)) != nullptr)
    {
        char *dirEnd = dirName + strlen(dirName);
        *dirEnd++ = PATH_SEPARATOR;
        *dirEnd = 0;
        int dirLen = strlen(dirName);
        while ((ent = readdir(dir)) != nullptr)
        {
            char *p = ent->d_name;
            if (p[0] == '.' && (p[1] == 0 || (p[1] == '.' && p[2] == 0)))
                continue;

            strcpy(dirEnd, p);
#ifdef _WIN32
            stat(dirName, &ss);
            if ((ss.st_mode & S_IFDIR) != 0)
#else
            if (ent->d_type == DT_DIR)
#endif
                listFiles(dirName, rc, strSize);
            else
            {
                rc.emplace_back(dirName);
                strSize += (dirLen + strlen(p));
            }
        }
        closedir(dir);
    }
}

template<class CONTAINER> int listFiles(const std::string &dirName, CONTAINER &rc)
{
    char path[16384];
    strcpy(path, dirName.c_str());
    int strSize = 0;
    listFiles(path, rc, strSize);
    return strSize;
}

void listFiles(char *dirName, std::function<void(const std::string &path)> &f);
void listFiles(const std::string &dirName, std::function<void(const std::string &path)> f);
void removeFiles(const std::string &dirName);

template<typename T> class UniQueue : public std::deque<std::reference_wrapper<const T> >
{
public:
    using KEY = std::reference_wrapper<const T>;
    using PARENT = std::deque<KEY>;

    void push_back(const T &value)
    {
        if (dirty)
            cleanup();
        auto r = unique.insert(value);
        KEY key = std::cref(*r.first);
        if (!r.second)
        { 
            // If already present we must remove the prevous one in the deck
            auto it = std::find_if(PARENT::begin(), PARENT::end(), [&](const KEY &k) -> bool
            {
                return key.get() == k.get();
            });
            PARENT::erase(it);
            unique.erase(r.first);
            r = unique.insert(value);
            key = std::cref(*r.first);
        }
        PARENT::push_back(key);
    }

    template<class ... S> void emplace_back(S && ... s)
    {
        if (dirty)
            cleanup();
        auto r = unique.emplace(std::forward<S>(s) ...);
        KEY key = std::cref(*r.first);
        if (!r.second)
        { // If already present we must remove the prevous one in the deck
            auto it = std::find_if(PARENT::begin(), PARENT::end(), [&](const KEY &k) -> bool
            {
                return key.get() == k.get();
            });
            PARENT::erase(it);
            unique.erase(r.first);
            r = unique.emplace(std::forward<S>(s) ...);
            key = std::cref(*r.first);
        }
        PARENT::push_back(key);
    }

    void pop_front()
    {
        dirty = true;
        PARENT::pop_front();
    }

	std::set<T>& data() { return unique; }

private:
    void cleanup()
    {
        fprintf(stderr, "Warning: Expensive to use UniQueue this way");
        // dirty = false;
    }

    std::set<T> unique;
    bool dirty = false;
};

uint32_t msdosToUnixTime(uint32_t m);
void makedir(const std::string &name);
void makedirs(const std::string &path);
std::string path_directory(const std::string &name);
std::string path_filename(const std::string &name);
std::string path_basename(const std::string &name);

bool startsWith(const std::string &name, const std::string &pref);


#endif // UTILS_H
