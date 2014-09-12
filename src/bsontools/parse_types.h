// Force = if doesn't parse, appends zero 
void appendAsNumberForce(bsonobjbuilder& b, const string &f, const char *p) {
    while (*p == ' ') p++;
    bool flt = false;
    {
        const char *q = p;
        if (*q == '-')
            q++;
        for (; *q; q++)
            if (!isdigit(*q))
                flt = true;
    }
    if (flt) {
        double result = 0;
        Status s = parseNumberFromString(p, &result);
            b.appendNumber(f, result);
    }
    else {
        long long n = 0;
        Status s = parseNumberFromString(p, &n);
            int x = (int)n;
            if (x == n) {
                b.appendNumber(f, x);
            }
            else {
                b.appendNumber(f, n);
            }
    }
}

struct Opts  {
    bool autoNum = false;
    bool floatPt = false;
};

bool appendAsNumber(bsonobjbuilder& b, const string &f, const char *p, const Opts& o) {
    while (*p == ' ') p++;
    bool flt = o.floatPt;
    if( !flt ) {
        const char *q = p;
        if (*q == '-')
            q++;
        for (; *q; q++)
            if (!isdigit(*q))
                flt = true;
    }
    if (flt) {
        double result = 0;
        Status s = parseNumberFromString(p, &result);
        if (s.isOK()) {
            b.appendNumber(f, result);
            return true;
        }
    }
    else {
        long long n = 0;
        Status s = parseNumberFromString(p, &n);
        if (s.isOK()){
            int x = (int)n;
            if (x == n) {
                b.appendNumber(f, x);
            }
            else {
                b.appendNumber(f, n);
            }
            return true;
        }
    }
    return false;
}

inline bool mightBeNumber(const char *p) {
    while (*p) {
        switch (*p) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
        case '-':
            break;
        default:
            return false;
        }
        p++;
    }
    return true;
}
