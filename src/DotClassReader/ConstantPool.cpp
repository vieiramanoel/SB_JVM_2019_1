#include <DotClassReader/ConstantPool.hpp>
#include <constants/Class.hpp>
#include <constants/Double.hpp>
#include <constants/Fieldref.hpp>
#include <constants/Float.hpp>
#include <constants/Integer.hpp>
#include <constants/InterfaceMethodref.hpp>
#include <constants/Long.hpp>
#include <constants/Methodref.hpp>
#include <constants/NameAndType.hpp>
#include <constants/String.hpp>
#include <constants/UTF8.hpp>
#include <cstring>

ConstantPool::ConstantPool(std::ifstream *file) {
    this->file = file;
    file->seekg(0);
    file->seekg(8);
    char constant_pool[2];
    auto ss = std::stringstream();
    file->read(constant_pool, 2);
    for (auto i = 0; i < 2; i++) {
        ss << static_cast<int>(static_cast<unsigned char>(constant_pool[i]));
    }

    pool_size = stoi(ss.str());
    // pool indexes start at 1, so we need to put a offset in our vector
    this->constant_pool.push_back(
        std::make_pair(-1, std::make_shared<float>(0)));
}

///
/// Parse the constant pool area of the .class file.
/// It fills the constant_pool vector
///
void ConstantPool::seek() {
    // go to file start
    file->seekg(0);
    file->seekg(10);
    // advance 10bytes
    for (auto i = 0; i < pool_size - 1; i++) {
        // do `pool_size` iterations over the file
        auto tag = getTag(file);
        // case double or float we need to skip one iteration
        if (tag == 5 || tag == 6)
            ++i;
        add_to_pool(tag);
    }
    resolve_pool();
}

///
/// Used by the method seek to fill constant_pool vector
/// For pool we need a vector of different types with different sizes
/// so we define as a vector of pairs (tag, void*)
/// where void* is a pointer for any structure that we need to handle
/// latter to convert back to its type, so first is the tag for the type
/// itself in go you could read this as
///
/// constantpool := []interface{}
///
/// for _, elem := range constantpool {
///
/// switch (elem.(type)) {
/// case cte_type:
///      .
///      .
///      .
///      .
///      .
///      .
/// }
///
void ConstantPool::add_to_pool(int tag) {
    switch (tag) {
    case 7: {
        // Class
        auto name_index = getInfo(file, 2);
        auto class_info = std::make_shared<Class>(name_index);
        constant_pool.push_back(std::make_pair(tag, class_info));
    } break;
    case 9: {
        // Fieldred_info
        auto class_index     = getInfo(file, 2);
        auto name_type_index = getInfo(file, 2);
        auto fieldref_info =
            std::make_shared<Fieldref>(class_index, name_type_index);
        constant_pool.push_back(std::make_pair(tag, fieldref_info));
    } break;
    case 12: {
        // NameAndType
        auto name_index       = getInfo(file, 2);
        auto descriptor_index = getInfo(file, 2);
        auto name_and_type_info =
            std::make_shared<NameAndType>(name_index, descriptor_index);
        constant_pool.push_back(std::make_pair(tag, name_and_type_info));
    } break;
    case 1: {
        // UTF8
        auto lenght    = getInfo(file, 2);
        auto data      = getUTF8Data(file, lenght);
        auto utf8_info = std::make_shared<UTF8>(lenght, data);
        constant_pool.push_back(std::make_pair(tag, utf8_info));
    } break;
    case 10: {
        // Methodref_info
        auto class_name_ref = getInfo(file, 2);
        auto name_type      = getInfo(file, 2);
        auto method_ref =
            std::make_shared<Methodref>(class_name_ref, name_type);
        constant_pool.push_back(std::make_pair(tag, method_ref));
    } break;
    case 11: {
        // InterfaceMethodref_info
        auto class_index         = getInfo(file, 2);
        auto name_and_type_index = getInfo(file, 2);
        auto interfaceMethodRef  = std::make_shared<InterfaceMethodref>(
            class_index, name_and_type_index);
        constant_pool.push_back(std::make_pair(tag, interfaceMethodRef));
    } break;
    case 8: {
        // String
        auto string_index = getInfo(file, 2);
        auto stringinfo   = std::make_shared<String>(string_index);
        constant_pool.push_back(std::make_pair(tag, stringinfo));
    } break;
    case 3: {
        // Integer
        auto value = getInfoRaw(file, 4);
        union int_bytes {
            unsigned char buf[4];
            int number;
        } integer_bytes;
        for (auto i = 0; i < 4; i++)
            integer_bytes.buf[i] = value[3 - i];
        auto integer = std::make_shared<Integer>(integer_bytes.number);
        constant_pool.push_back(std::make_pair(tag, integer));
    } break;
    case 4: {
        // Float
        auto value = getInfoRaw(file, 4);
        union float_bytes {
            unsigned char buf[4];
            float number;
        } fvalue;
        for (auto i = 0; i < 4; i++)
            fvalue.buf[i] = value[3 - i];

        auto float_info = std::make_shared<Float>(fvalue.number);
        constant_pool.push_back(std::make_pair(tag, float_info));
    } break;
    case 5: {
        // Long
        auto high_bytes = getInfoRaw(file, 4);
        auto low_bytes  = getInfoRaw(file, 4);
        auto long_info  = std::make_shared<Long>(high_bytes, low_bytes);
        constant_pool.push_back(std::make_pair(tag, long_info));
        constant_pool.push_back(std::make_pair(-1, std::make_shared<float>(0)));
    } break;
    case 6: {
        // Double
        auto high_bytes  = getInfoRaw(file, 4);
        auto low_bytes   = getInfoRaw(file, 4);
        auto double_info = std::make_shared<Double>(high_bytes, low_bytes);
        constant_pool.push_back(std::make_pair(tag, double_info));
        constant_pool.push_back(std::make_pair(-1, std::make_shared<float>(0)));
    } break;
    default:
        char msg_error[30];
        sprintf(msg_error, "Tag %d is not in scope", tag);
        throw std::domain_error(msg_error);
        break;
    }
}

/// Show ConstantPool entries.
/// For all elements in constant pool we decode from void* to tag_type
/// associated using static_pointer_cast to convert back to proper type, and
/// handling each one as necessary
void ConstantPool::show() {

    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "               Constant Pool" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    int i = 1;
    for (auto elem = constant_pool.begin() + 1; elem < constant_pool.end();
         elem++) {
        std::cout << "[" << i << "] ";
        switch (elem->first) {
        case 7: {
            auto class_info = std::static_pointer_cast<Class>(elem->second);
            std::cout << "Class_info:" << std::endl
                      << "Name index: #" << class_info->name_index << " "
                      << class_info->name << std::endl
                      << std::endl;
        } break;
        case 9: {
            auto fieldref_info =
                std::static_pointer_cast<Fieldref>(elem->second);
            std::cout << "Fieldref_info:" << std::endl
                      << "Class index: #" << fieldref_info->class_index << " "
                      << fieldref_info->class_name << std::endl
                      << "Name and Type index: #"
                      << fieldref_info->name_type_index << " "
                      << fieldref_info->name_and_type << std::endl
                      << std::endl;
        } break;
        case 12: {
            auto name_type_info =
                std::static_pointer_cast<NameAndType>(elem->second);
            std::cout << "NameAndType_info:" << std::endl
                      << "Name index: #" << name_type_info->name_index << " "
                      << name_type_info->name << std::endl
                      << "Descriptor index: #"
                      << name_type_info->descriptor_index << " "
                      << name_type_info->descriptor << std::endl
                      << std::endl;
        } break;
        case 1: {
            auto utf8_info = std::static_pointer_cast<UTF8>(elem->second);
            std::cout << "UTF8_info: " << std::endl
                      << "lenght: " << utf8_info->lenght << std::endl
                      << "Data: " << utf8_info->bytes << std::endl
                      << std::endl;
        } break;
        case 10: {
            auto method_ref = std::static_pointer_cast<Methodref>(elem->second);
            std::cout << "Methodref_info:" << std::endl
                      << "Class Name: #" << method_ref->class_index << " "
                      << method_ref->class_name << std::endl
                      << "Name and Type: #" << method_ref->name_type_index
                      << " " << method_ref->name_and_type << std::endl
                      << std::endl;
        } break;
        case 11: {
            auto interface_info =
                std::static_pointer_cast<InterfaceMethodref>(elem->second);
            std::cout << "InterfaceMethodref_info:" << std::endl
                      << "Classe index: #" << interface_info->class_index << " "
                      << interface_info->class_name << std::endl
                      << "Name and Type index: #"
                      << interface_info->name_type_index << " "
                      << interface_info->name_and_type << std::endl
                      << std::endl;

        } break;
        case 8: {
            auto string_info = std::static_pointer_cast<String>(elem->second);
            std::cout << "String_info:" << std::endl
                      << "String index: #" << string_info->string_index << " "
                      << string_info->string << std::endl
                      << std::endl;
        } break;
        case 3: {
            auto integer_info = std::static_pointer_cast<Integer>(elem->second);
            std::cout << "Integer_info:" << std::endl
                      << "bytes: " << integer_info->value << std::endl
                      << std::endl;
        } break;
        case 4: {
            auto float_info = std::static_pointer_cast<Float>(elem->second);
            std::cout << "Float_info:" << std::endl
                      << "bytes: " << float_info->value << std::endl
                      << std::endl;
            i++;
        } break;
        case 5: {
            auto long_info = std::static_pointer_cast<Long>(elem->second);
            std::cout << "Long_info:" << std::endl << std::endl;
            i++;
            std::cout << "[" << i << "] Long continuation\n\n";
            elem++;
        } break;
        case 6: {
            auto double_info = std::static_pointer_cast<Double>(elem->second);
            std::cout << "Double_info" << std::endl
                      << "high bytes: " << double_info->high_bytes << std::endl
                      << "low bytes: " << double_info->low_bytes << std::endl
                      << "Value: " << double_info->getValue() << std::endl
                      << std::endl;
            i++;
            std::cout << "[" << i << "] Double continuation\n\n";
            elem++;
        } break;
        case -1:
            continue;
            break;
        default:
            char msg_error[30];
            sprintf(msg_error, "Tag %d is not in scope", elem->first);
            throw std::domain_error(msg_error);
            break;
        }
        i++;
    }
    std::cout << std::endl;
}

/// Used by seek after it has alreay read all constant pool values.
/// It names the created variables in add_to_pool whose names are
/// stored in other constant pool entries
void ConstantPool::resolve_pool() {
    for (auto i = 1; i < constant_pool.size() - 2; i++) {
        if (constant_pool[i].first != 3 && constant_pool[i].first != 4 &&
            constant_pool[i].first != 5 && constant_pool[i].first != 6 &&
            constant_pool[i].first != -1) {
            // those should not be resolved
            resolve(i);
        }
    }
}

/// Used by resolve_pool in each interation
std::string ConstantPool::resolve(int idx) {
    auto tag      = constant_pool[idx].first;
    auto constant = constant_pool[idx].second;
    if (tag == -1) {
        throw std::invalid_argument("Wrong index for constant pool");
    }
    if (tag != 1) {
        switch (tag) {
        case 7: {
            auto class_info  = std::static_pointer_cast<Class>(constant);
            class_info->name = resolve(class_info->name_index);
        } break;
        case 9: {
            auto fieldref_info = std::static_pointer_cast<Fieldref>(constant);
            fieldref_info->class_name = resolve(fieldref_info->class_index);
            fieldref_info->name_and_type =
                resolve(fieldref_info->name_type_index);
        } break;
        case 12: {
            auto name_type_info =
                std::static_pointer_cast<NameAndType>(constant);
            name_type_info->name = resolve(name_type_info->name_index);
            name_type_info->descriptor =
                resolve(name_type_info->descriptor_index);
            return name_type_info->name + name_type_info->descriptor;
        } break;
        case 10: {
            auto method_ref = std::static_pointer_cast<Methodref>(constant);
            method_ref->name_and_type = resolve(method_ref->name_type_index);
            method_ref->class_name    = resolve(method_ref->class_index);
        } break;
        case 11: {
            auto interface_info =
                std::static_pointer_cast<InterfaceMethodref>(constant);
            interface_info->class_name = resolve(interface_info->class_index);
            interface_info->name_and_type =
                resolve(interface_info->name_type_index);
        } break;
        case 8: {
            auto string_info    = std::static_pointer_cast<String>(constant);
            string_info->string = resolve(string_info->string_index);
        } break;
        default:
            char msg_error[30];
            sprintf(msg_error, "Tag %d is not in scope", tag);
            throw std::domain_error(msg_error);
            break;
        }
    }
    auto utf8_info = std::static_pointer_cast<UTF8>(constant);
    return utf8_info->bytes;
}

/// It receives a index to a constant pool Methodref_info entry
/// (tag = 10, represented by Methodref.hpp) and returns the method name.
/// If the index apoints to other tag, the method is aborted.
std::string ConstantPool::getMethodNameByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[50];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }
    if (constant_pool[index].first != 10) {
        char error[150];
        sprintf(error,
                "Requested descriptor index %d is not a valid "
                "Methodref, is a %d instead",
                index, constant_pool[index].first);
        throw std::invalid_argument(error);
    }
    auto methref =
        std::static_pointer_cast<Methodref>(constant_pool[index].second);
    return methref->name_and_type;
}

/// It receives a index to a constant pool Methodref_info entry
/// (tag = 10, represented by Methodref.hpp) and returns the method name index.
/// If the index apoints to other tag, the method is aborted.
/// If the class name is java/lang/Object it returns -1
/// If the class name is java/io/PrintStream it returns -2
int ConstantPool::getMethodNameIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[50];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }
    if (constant_pool[index].first != 10) {
        char error[150];
        sprintf(error,
                "Requested descriptor index %d is not a valid "
                "Methodref, is a %d instead",
                index, constant_pool[index].first);
        throw std::invalid_argument(error);
    }
    auto methref =
        std::static_pointer_cast<Methodref>(constant_pool[index].second);
    auto name_ref = std::static_pointer_cast<NameAndType>(
        constant_pool[methref->name_type_index].second);
    if (methref->class_name == "java/lang/Object") {
        return -1; // Means the Executer will have to solve java/lang/object
    }
    if (methref->class_name == "java/io/PrintStream") {
        return -2; // Means the Executer will have to solve
                   // java/io/PrintStream
    }
    auto name_index = name_ref->name_index;
    return name_index;
}

/// It gets a index to the context pool and return its name if its pointing to a
/// Utf8, Class, Methodref, Fieldref, String,
/// Double, Float, Integer, Long or InterfaceMethodref.
std::string ConstantPool::getNameByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[50];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }

    if (constant_pool[index].first != 1 && constant_pool[index].first != 7 &&
        constant_pool[index].first != 10 && constant_pool[index].first != 9 &&
        constant_pool[index].first != 8 && constant_pool[index].first != 6 &&
        constant_pool[index].first != 5 && constant_pool[index].first != 4 &&
        constant_pool[index].first != 11 && constant_pool[index].first != 3) {
        char error[150];
        sprintf(error,
                "Requested descriptor index %d is not a valid "
                "Utf8, Class, Methodref, Fieldref, "
                "String, Double, Float, Integer, Long or "
                "InterfaceMethodref on constant_pool, is a %d instead",
                index, constant_pool[index].first);
        throw std::invalid_argument(error);
    }

    switch (constant_pool[index].first) {
    case 1: {
        auto name =
            std::static_pointer_cast<UTF8>(constant_pool[index].second)->bytes;
        return name;
    } break;
    case 7: {
        auto name =
            std::static_pointer_cast<Class>(constant_pool[index].second)->name;
        return name;
    } break;
    case 10: {
        auto methref =
            std::static_pointer_cast<Methodref>(constant_pool[index].second);
        auto name =
            "<" + methref->class_name + "/" + methref->name_and_type + ">";
        return name;
    } break;
    case 9: {
        auto fieldref =
            std::static_pointer_cast<Fieldref>(constant_pool[index].second);
        auto name =
            "<" + fieldref->class_name + "/" + fieldref->name_and_type + ">";
        return name;
    } break;
    case 8: {
        auto name =
            std::static_pointer_cast<String>(constant_pool[index].second)
                ->string;
        return name;
    } break;
    case 6: {
        auto d = std::static_pointer_cast<Double>(constant_pool[index].second)
                     ->getValue();
        std::stringstream ss;
        ss << d;
        return ss.str();
    } break;
    case 4: {
        auto f =
            std::static_pointer_cast<Float>(constant_pool[index].second)->value;
        std::stringstream ss;
        ss << f;
        return ss.str();
    } break;
    case 3: {
        auto i = std::static_pointer_cast<Integer>(constant_pool[index].second)
                     ->value;
        std::stringstream ss;
        ss << i;
        return ss.str();
    }
    case 5: {
        auto l = std::static_pointer_cast<Long>(constant_pool[index].second)
                     ->getValue();
        std::stringstream ss;
        ss << l;
        return ss.str();
    } break;
    case 11: {
        auto interfaceRef = std::static_pointer_cast<InterfaceMethodref>(
            constant_pool[index].second);
        return "<" + interfaceRef->class_name + "/" +
               interfaceRef->name_and_type + ">";
    }
    }
    return "";
}

/// Return the UTF-8 constant pool entry with name "LineNuberTable" witch is
/// used by debbuger (but not implemented on this code)
int ConstantPool::getLineTableIndex() {
    for (int i = 1; i < constant_pool.size() - 1; i++) {
        if (constant_pool[i].first == 1) {
            auto utf8 = std::static_pointer_cast<UTF8>(constant_pool[i].second);
            if (utf8->bytes == "LineNumberTable") {
                return i;
            }
        }
    }
    return -1;
}

/// Return the UTF-8 constant pool entry with name "Code". It is used by other
/// methods to know if some constant pool entry is the type of code.
int ConstantPool::getCodeIndex() {
    for (int i = 1; i < constant_pool.size() - 1; i++) {
        if (constant_pool[i].first == 1) {
            auto utf8 = std::static_pointer_cast<UTF8>(constant_pool[i].second);
            if (utf8->bytes == "Code") {
                return i;
            }
        }
    }
    return -1;
}

/// Return the constant pool size
int ConstantPool::cpCount() { return pool_size; }

/// If the index points to a Long or a Double entry on the ConstantPool, returns
/// a DoubleLong (whitch represents a double or a long variable)
DoubleLong ConstantPool::getNumberByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[50];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }
    switch (constant_pool[index].first) {
    case 6: {
        auto number =
            std::static_pointer_cast<Double>(constant_pool[index].second)
                ->getValue();
        DoubleLong dl;
        dl.t     = D;
        dl.val.d = number;
        return dl;
    } break;
    case 5: {
        auto number =
            std::static_pointer_cast<Long>(constant_pool[index].second)
                ->getValue();
        DoubleLong dl;
        dl.t     = J;
        dl.val.l = number;
        return dl;
    } break;
    default:
        throw std::runtime_error("Requested index is not a double nor long");
    }
    return DoubleLong{};
}

/// Returns a string with the field name and type if the
/// index points to a Methodref entry on the ConstantPool
std::string ConstantPool::getNameAndTypeByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[80];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }

    if (constant_pool[index].first == 10) {
        auto methref =
            std::static_pointer_cast<Methodref>(constant_pool[index].second);
        return methref->name_and_type;
    } else {
        throw std::runtime_error(
            "Requested index is not a reference to a method");
    }
    return "";
}

/// If the index points to a Integer or a Float entry on the ConstantPool,
/// returns a IntFloatReference (whitch represents a int or a float variable)
IntFloatReference ConstantPool::getValueByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[80];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }

    switch (constant_pool[index].first) {
    case 3: {
        auto intref =
            std::static_pointer_cast<Integer>(constant_pool[index].second);
        IntFloatReference ifr;
        ifr.t     = I;
        ifr.val.i = intref->value;
        return ifr;
    } break;
    case 4: {
        auto floatref =
            std::static_pointer_cast<Float>(constant_pool[index].second);
        IntFloatReference ifr;
        ifr.t     = F;
        ifr.val.f = floatref->value;
        return ifr;
    }
    case 8: {
        auto strref =
            std::static_pointer_cast<String>(constant_pool[index].second);
        IntFloatReference ifr{.t = R};
        ifr.str_value = strref->string;
        return ifr;
    } break;
    default:
        throw std::runtime_error(
            "Requested index is neither a int nor float nor string");
    }
    return IntFloatReference{};
}

/// If the index points to a Fieldref entry on the ConstantPool,
/// returns a string that represents the field name
std::string ConstantPool::getFieldByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[80];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }
    if (constant_pool[index].first != 9) {
        throw std::runtime_error("index is not a FieldRef");
    }
    auto fieldRef =
        std::static_pointer_cast<Fieldref>(constant_pool[index].second);
    auto name_and_type = std::static_pointer_cast<NameAndType>(
        constant_pool[fieldRef->name_type_index].second);
    return name_and_type->name;
}

std::vector<std::string>
ConstantPool::getExternalClasses(std::string this_class) {
    std::vector<std::string> external_classes;
    for (auto elem : this->constant_pool) {
        if (elem.first == 7) {
            auto class_ref = std::static_pointer_cast<Class>(elem.second);
            if (this_class !=
                    class_ref->name && // if it is not a java/.. function and
                                       // class_ref->name != this class it might
                                       // be an external class
                class_ref->name.find("java") == std::string::npos) {
                auto bracket = class_ref->name.find_first_of('[');
                if (bracket == std::string::npos || bracket != 0) {
                    int posix = class_ref->name.find(
                        "["); // but it also cant begin with a '['.
                    if (posix != 0) {
                        external_classes.push_back(class_ref->name);
                    }
                }
            }
        }
    }
    return external_classes;
}
std::string ConstantPool::getClassNameFromMethodByIndex(int index) {
    if (index > constant_pool.size() - 1 || index == 0) {
        char error[50];
        sprintf(error,
                "Requested index %d is out of range, allowed range: 1-%ld",
                index, constant_pool.size() - 1);
        throw std::invalid_argument(error);
    }
    if (constant_pool[index].first != 10) {
        char error[150];
        sprintf(error,
                "Requested descriptor index %d is not a valid "
                "Methodref, is a %d instead",
                index, constant_pool[index].first);
        throw std::invalid_argument(error);
    }
    auto methref =
        std::static_pointer_cast<Methodref>(constant_pool[index].second);
    return methref->class_name;
}

int ConstantPool::getMethodIndexByName(std::string method_name) {
    auto i = 0;
    for (auto elem : constant_pool) {
        if (elem.first == 1) {
            auto name_ref = std::static_pointer_cast<UTF8>(elem.second);
            if (name_ref->bytes == method_name) {
                return i;
            }
        }
        i++;
    }
    return -1;
}