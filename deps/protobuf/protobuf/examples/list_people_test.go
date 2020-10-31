package main

import (
	"bytes"
	"strings"
	"testing"

	pb "github.com/protocolbuffers/protobuf/examples/tutorial"
)

func TestWritePersonWritesPerson(t *testing.T) {
	buf := new(bytes.Buffer)
	// [START populate_proto]
	p := pb.Person{
		Id:    1234,
		Name:  "John Doe",
		Email: "jdoe@example.com",
		Phones: []*pb.Person_PhoneNumber{
			{Number: "555-4321", Type: pb.Person_HOME},
		},
	}
	// [END populate_proto]
	writePerson(buf, &p)
	got := buf.String()
	want := `Person ID: 1234
  Name: John Doe
  E-mail address: jdoe@example.com
  Home phone #: 555-4321
`
	if got != want {
		t.Errorf("writePerson(%s) =>\n\t%q, want %q", p.String(), got, want)
	}
}

func TestListPeopleWritesList(t *testing.T) {
	buf := new(bytes.Buffer)
	in := pb.AddressBook{People: []*pb.Person{
		{
			Name:  "John Doe",
			Id:    101,
			Email: "john@example.com",
		},
		{
			Name: "Jane Doe",
			Id:   102,
		},
		{
			Name:  "Jack Doe",
			Id:    201,
			Email: "jack@example.com",
			Phones: []*pb.Person_PhoneNumber{
				{Number: "555-555-5555", Type: pb.Person_WORK},
			},
		},
		{
			Name:  "Jack Buck",
			Id:    301,
			Email: "buck@example.com",
			Phones: []*pb.Person_PhoneNumber{
				{Number: "555-555-0000", Type: pb.Person_HOME},
				{Number: "555-555-0001", Type: pb.Person_MOBILE},
				{Number: "555-555-0002", Type: pb.Person_WORK},
			},
		},
		{
			Name:  "Janet Doe",
			Id:    1001,
			Email: "janet@example.com",
			Phones: []*pb.Person_PhoneNumber{
				{Number: "555-777-0000"},
				{Number: "555-777-0001", Type: pb.Person_HOME},
			},
		},
	}}
	listPeople(buf, &in)
	want := strings.Split(`Person ID: 101
  Name: John Doe
  E-mail address: john@example.com
Person ID: 102
  Name: Jane Doe
Person ID: 201
  Name: Jack Doe
  E-mail address: jack@example.com
  Work phone #: 555-555-5555
Person ID: 301
  Name: Jack Buck
  E-mail address: buck@example.com
  Home phone #: 555-555-0000
  Mobile phone #: 555-555-0001
  Work phone #: 555-555-0002
Person ID: 1001
  Name: Janet Doe
  E-mail address: janet@example.com
  Mobile phone #: 555-777-0000
  Home phone #: 555-777-0001
`, "\n")
	got := strings.Split(buf.String(), "\n")
	if len(got) != len(want) {
		t.Errorf(
			"listPeople(%s) =>\n\t%q has %d lines, want %d",
			in.String(),
			buf.String(),
			len(got),
			len(want))
	}
	lines := len(got)
	if lines > len(want) {
		lines = len(want)
	}
	for i := 0; i < lines; i++ {
		if got[i] != want[i] {
			t.Errorf(
				"listPeople(%s) =>\n\tline %d %q, want %q",
				in.String(),
				i,
				got[i],
				want[i])
		}
	}
}
