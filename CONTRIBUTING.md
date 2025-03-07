import React, { useState } from 'react';
import { View, Text, Button, TextInput, FlatList, TouchableOpacity } from 'react-native';
import MapView, { Marker } from 'react-native-maps';

const App = () => {
  const [searchQuery, setSearchQuery] = useState('');
  const parties = [
    { name: 'Downtown Bash', vibe: 'Wild', location: 'Downtown', latitude: 40.7128, longitude: -74.0060, details: 'Live DJ, Free Drinks', price: '$20', paymentLink: 'https://pay.example.com/downtownbash' },
    { name: 'Beach Party', vibe: 'Chill', location: 'Coastline', latitude: 34.0194, longitude: -118.4912, details: 'Sunset Vibes, BBQ, Bonfire', price: 'Free', paymentLink: '' },
    { name: 'Club Night', vibe: 'Exclusive', location: 'City Center', latitude: 41.8781, longitude: -87.6298, details: 'VIP Access, Bottle Service', price: '$50', paymentLink: 'https://pay.example.com/clubnight' }
  ];

  const parkingSpots = [
    { name: 'Main Street Parking', latitude: 40.7138, longitude: -74.0050 },
    { name: 'Beachside Lot', latitude: 34.0200, longitude: -118.4900 }
  ];

  const [chats, setChats] = useState([]);
  const [following, setFollowing] = useState([]);

  const handlePurchase = (paymentLink) => {
    if (paymentLink) {
      alert(`Redirecting to payment: ${paymentLink}`);
    } else {
      alert('This event is free or does not require payment.');
    }
  };

  const handleAdClick = () => {
    alert('Viewing sponsored event...');
  };

  const handleUberRequest = () => {
    alert('Opening Uber for ride request...');
  };

  const createChat = (user) => {
    setChats([...chats, { user, messages: [] }]);
    alert(`Chat started with ${user}`);
  };

  const followUserOrPlace = (name) => {
    setFollowing([...following, name]);
    alert(`Following ${name}`);
  };

  return (
    <View style={{ flex: 1, justifyContent: 'center', alignItems: 'center' }}>
      <Text style={{ fontSize: 24, fontWeight: 'bold' }}>VibeHunt</Text>
      <TextInput 
        placeholder='Search for parties...'
        style={{ borderWidth: 1, width: '80%', margin: 10, padding: 8 }}
        value={searchQuery}
        onChangeText={setSearchQuery}
      />
      <Button title='Find Your Vibe' onPress={() => alert('Searching for parties...')} />
      <FlatList
        data={parties}
        keyExtractor={(item, index) => index.toString()}
        renderItem={({ item }) => (
          <TouchableOpacity onPress={() => alert(`Party: ${item.name}\nVibe: ${item.vibe}\nLocation: ${item.location}\nDetails: ${item.details}\nPrice: ${item.price}`)}>
            <Text style={{ margin: 10, fontSize: 18 }}>{item.name} - {item.vibe}</Text>
            {item.paymentLink && <Button title='Buy Ticket' onPress={() => handlePurchase(item.paymentLink)} />}
            <Button title='Follow Event' onPress={() => followUserOrPlace(item.name)} />
          </TouchableOpacity>
        )}
      />
      
      {/* Map Feature with Parking Spots */}
      <MapView 
        style={{ width: '100%', height: 300, marginTop: 20 }} 
        initialRegion={{ latitude: 37.7749, longitude: -122.4194, latitudeDelta: 0.1, longitudeDelta: 0.1 }}>
        {parties.map((party, index) => (
          <Marker 
            key={index} 
            coordinate={{ latitude: party.latitude, longitude: party.longitude }}
            title={party.name}
            description={`${party.vibe} - ${party.location}`}
          />
        ))}
        {parkingSpots.map((spot, index) => (
          <Marker 
            key={`parking-${index}`} 
            coordinate={{ latitude: spot.latitude, longitude: spot.longitude }}
            title={spot.name}
            description='Parking Spot'
            pinColor='blue'
          />
        ))}
      </MapView>
      
      {/* Uber Ride Request Feature */}
      <Button title='Request an Uber' onPress={handleUberRequest} />
      
      {/* Monetization Features */}
      <Button title='Upgrade to Premium' onPress={() => alert('Upgrading to premium...')} />
      <TouchableOpacity onPress={handleAdClick}>
        <Text style={{ marginTop: 20, fontSize: 16, color: 'blue' }}>Sponsored Party - Click to View</Text>
      </TouchableOpacity>
      
      {/* Chat and Follow Features */}
      <Button title='Start a Chat' onPress={() => createChat('User123')} />
      <FlatList
        data={chats}
        keyExtractor={(item, index) => index.toString()}
        renderItem={({ item }) => (
          <Text style={{ margin: 10, fontSize: 18 }}>Chat with {item.user}</Text>
        )}
      />
      
      {/* New Feature: Group Chats */}
      <Button title='Create Group Chat' onPress={() => createChat('Group Chat')} />
      
      {/* New Feature: Location-based Events */}
      <Text style={{ fontSize: 18, fontWeight: 'bold', marginTop: 20 }}>Events Near You</Text>
      <FlatList
        data={parties}
        keyExtractor={(item, index) => index.toString()}
        renderItem={({ item }) => (
          <Text style={{ margin: 10, fontSize: 16 }}>{item.name} - {item.location}</Text>
        )}
      />
    </View>
  );
};

export default App;
