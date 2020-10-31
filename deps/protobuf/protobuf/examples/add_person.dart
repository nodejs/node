import 'dart:io';

import 'dart_tutorial/addressbook.pb.dart';

// This function fills in a Person message based on user input.
Person promptForAddress() {
  Person person = Person();

  print('Enter person ID: ');
  String input = stdin.readLineSync();
  person.id = int.parse(input);

  print('Enter name');
  person.name = stdin.readLineSync();

  print('Enter email address (blank for none) : ');
  String email = stdin.readLineSync();
  if (email.isNotEmpty) {
    person.email = email;
  }

  while (true) {
    print('Enter a phone number (or leave blank to finish): ');
    String number = stdin.readLineSync();
    if (number.isEmpty) break;

    Person_PhoneNumber phoneNumber = Person_PhoneNumber();

    phoneNumber.number = number;
    print('Is this a mobile, home, or work phone? ');

    String type = stdin.readLineSync();
    switch (type) {
      case 'mobile':
        phoneNumber.type = Person_PhoneType.MOBILE;
        break;
      case 'home':
        phoneNumber.type = Person_PhoneType.HOME;
        break;
      case 'work':
        phoneNumber.type = Person_PhoneType.WORK;
        break;
      default:
        print('Unknown phone type.  Using default.');
    }
    person.phones.add(phoneNumber);
  }

  return person;
}

// Reads the entire address book from a file, adds one person based
// on user input, then writes it back out to the same file.
main(List<String> arguments) {
  if (arguments.length != 1) {
    print('Usage: add_person ADDRESS_BOOK_FILE');
    exit(-1);
  }

  File file = File(arguments.first);
  AddressBook addressBook;
  if (!file.existsSync()) {
    print('File not found. Creating new file.');
    addressBook = AddressBook();
  } else {
    addressBook = AddressBook.fromBuffer(file.readAsBytesSync());
  }
  addressBook.people.add(promptForAddress());
  file.writeAsBytes(addressBook.writeToBuffer());
}
