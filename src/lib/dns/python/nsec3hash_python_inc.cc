namespace {
// Modifications
//   - removed intermediate details note, mainly for brevity
//   - removed std::bad_alloc
const char* const NSEC3Hash_doc = "\
A calculator of NSEC3 hashes.\n\
\n\
This is a simple class that encapsulates the algorithm of calculating\n\
NSEC3 hash values as defined in RFC5155.\n\
\n\
NSEC3Hash(param)\n\
\n\
    Constructor.\n\
\n\
    The hash algorithm given via param must be known to the\n\
    implementation. Otherwise UnknownNSEC3HashAlgorithm exception will\n\
    be thrown.\n\
\n\
    Exceptions:\n\
      UnknownNSEC3HashAlgorithm The specified algorithm in param is\n\
                 unknown.\n\
\n\
    Parameters:\n\
      param      NSEC3PARAM or NSEC3 Rdata object whose parameters are\n\
                 to be used for subsequent calculation.\n\
\n\
";

const char* const NSEC3Hash_calculate_doc = "\
calculate(name) -> string\n\
\n\
Calculate the NSEC3 hash.\n\
\n\
This method calculates the NSEC3 hash value for the given name with\n\
the hash parameters (algorithm, iterations and salt) given at\n\
construction, and returns the value in a base32hex-encoded string\n\
(without containing any white spaces). All US-ASCII letters in the\n\
string will be upper cased.\n\
\n\
Parameters:\n\
  name       The domain name for which the hash value is to be\n\
             calculated.\n\
\n\
Return Value(s): Base32hex-encoded string of the hash value.\n\
";

const char* const NSEC3Hash_match_doc = "\
match(rdata) -> bool\n                   \
\n\
Match given NSEC3 or NSEC3PARAM parameters with that of the hash.\n\
\n\
This method compares NSEC3 parameters used for hash calculation in the\n\
object with those in the given RDATA, and return true iff they\n\
completely match. In the current implementation only the algorithm,\n\
iterations and salt are compared; the flags are ignored (as they don't\n\
affect hash calculation per RFC5155).\n\
\n\
Exceptions:\n\
  None\n\
\n\
Parameters:\n\
  rdata      An NSEC3 or NSEC3PARAM Rdata object whose hash parameters\n\
             are to be matched\n\
\n\
Return Value(s): true If the given parameters match the local ones;\n\
false otherwise.\n\
";
} // unnamed namespace
