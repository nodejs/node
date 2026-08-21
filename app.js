// ==========================================
// Interactive Background Tracker
// ==========================================
function updateBackgroundCoordinates(e) {
    // Get mouse or touch coordinates
    let clientX = e.clientX;
    let clientY = e.clientY;

    // If it's a touch screen, get the first finger's position
    if (e.touches && e.touches.length > 0) {
        clientX = e.touches[0].clientX;
        clientY = e.touches[0].clientY;
    }

    // Calculate percentage across the screen
    const x = (clientX / window.innerWidth) * 100;
    const y = (clientY / window.innerHeight) * 100;

    // Send coordinates to the CSS variables
    document.documentElement.style.setProperty('--mouseX', `${x}%`);
    document.documentElement.style.setProperty('--mouseY', `${y}%`);
}

// Listen for mouse movement on computers
document.addEventListener('mousemove', updateBackgroundCoordinates);

// Listen for touch/swiping on mobile phones
document.addEventListener('touchmove', updateBackgroundCoordinates);


// ==========================================
// Main Application Logic
// ==========================================
// Wait for the DOM to fully load before running the script
document.addEventListener('DOMContentLoaded', () => {
    
    // 1. Grab all the elements we need to interact with
    const detectBtn = document.getElementById('detectBtn');
    const btnText = document.getElementById('btnText');
    const loader = document.getElementById('loader');
    const resultSection = document.getElementById('resultSection');
    
    const deviceNameEl = document.getElementById('deviceName');
    const deviceYearEl = document.getElementById('deviceYear');
    const devicePriceEl = document.getElementById('devicePrice');

    // 2. Add click event listener to the main button (WITH SAFETY CHECK)
    if (detectBtn) {
        detectBtn.addEventListener('click', () => {
            // UI State: Show loading animation
            btnText.textContent = 'Detecting...';
            loader.style.display = 'inline-block';
            resultSection.style.display = 'none';
            detectBtn.disabled = true;

            // Simulate a slight delay so it feels like it's scanning hardware (1.5 seconds)
            setTimeout(() => {
                const deviceInfo = detectDevice();
                const estimatedPrice = calculatePrice(deviceInfo);

                // Update the HTML with the results
                deviceNameEl.textContent = deviceInfo.name;
                deviceYearEl.textContent = `Estimated Year: ${deviceInfo.year}`;
                devicePriceEl.textContent = `₹${estimatedPrice}`; 

                // UI State: Hide loader, show results
                btnText.textContent = 'Detect My Device Again';
                loader.style.display = 'none';
                resultSection.style.display = 'block';
                detectBtn.disabled = false;
            }, 1500);
        });
    }

    // 3. Function to detect the device using the browser's User-Agent
    function detectDevice() {
        const ua = navigator.userAgent;
        let deviceName = "Generic Smartphone";
        let releaseYear = "2020"; // Default fallback

        // Basic detection logic
        if (/iPhone/i.test(ua)) {
            deviceName = "Apple iPhone";
            // iOS version can sometimes hint at the year
            if (/OS 17_/i.test(ua)) releaseYear = "2023";
            else if (/OS 16_/i.test(ua)) releaseYear = "2022";
            else if (/OS 15_/i.test(ua)) releaseYear = "2021";
            else releaseYear = "2020 or older";
        } 
        else if (/Samsung/i.test(ua) || /SM-/i.test(ua)) {
            deviceName = "Samsung Galaxy Device";
            releaseYear = "2022";
        }
        else if (/Pixel/i.test(ua)) {
            deviceName = "Google Pixel";
            releaseYear = "2022";
        }
        else if (/OnePlus/i.test(ua)) {
            deviceName = "OnePlus Device";
            releaseYear = "2021";
        }
        else if (/Android/i.test(ua)) {
            deviceName = "Android Smartphone";
            releaseYear = "2021";
        }
        else if (/Windows NT|Macintosh|Linux/i.test(ua)) {
            deviceName = "Desktop / Laptop Computer";
            releaseYear = "N/A (Phones only)";
        }

        return {
            name: deviceName,
            year: releaseYear
        };
    }

    // 4. Function to calculate a mock price based on the detected device
    function calculatePrice(device) {
        let basePrice = 5000; // Base value for unknown/generic devices

        // Adjust price based on brand
        if (device.name.includes('iPhone')) {
            basePrice = 25000;
        } else if (device.name.includes('Samsung')) {
            basePrice = 15000;
        } else if (device.name.includes('Pixel')) {
            basePrice = 18000;
        } else if (device.name.includes('OnePlus')) {
            basePrice = 14000;
        } else if (device.name.includes('Desktop')) {
            return "0 (We only buy mobiles)";
        }

        // Add logic based on year if available
        if (device.year === "2023") basePrice += 10000;
        if (device.year === "2022") basePrice += 5000;
        if (device.year === "2021") basePrice -= 2000;
        
        // Format the number with commas (e.g., 25,000)
        return basePrice.toLocaleString('en-IN');
    }
});
// ==========================================
// Modal Form Logic for Selling / Donating
// ==========================================
document.addEventListener('DOMContentLoaded', () => {
    const modal = document.getElementById('formModal');
    const closeModal = document.getElementById('closeModal');
    const donateBtn = document.getElementById('donateBtn');
    const sellBtn = document.getElementById('sellBtn');
    const modalTitle = document.getElementById('modalTitle');
    const modalSubtitle = document.getElementById('modalSubtitle');
    const pickupForm = document.getElementById('pickupForm');

    // Open Modal for Donating
    if (donateBtn) {
        donateBtn.addEventListener('click', () => {
            modalTitle.textContent = "🌍 Donate & Save Environment";
            modalSubtitle.textContent = "Thank you for reducing e-waste! Enter your details for a 100% free home pickup.";
            modal.style.display = 'flex';
        });
    }

    // Open Modal for Selling
    if (sellBtn) {
        sellBtn.addEventListener('click', () => {
            modalTitle.textContent = "💵 Get Instant Cash";
            modalSubtitle.textContent = "Enter your details to lock in your device valuation and schedule free cash pickup.";
            modal.style.display = 'flex';
        });
    }

    // Close Modal when clicking 'X'
    if (closeModal) {
        closeModal.addEventListener('click', () => {
            modal.style.display = 'none';
        });
    }

    // Close Modal when clicking outside the card
    window.addEventListener('click', (e) => {
        if (e.target === modal) {
            modal.style.display = 'none';
        }
    });

    // Handle Form Submission
    if (pickupForm) {
        pickupForm.addEventListener('submit', (e) => {
            e.preventDefault();
            alert("🎉 Success! Your free home pickup has been scheduled. Our executive will arrive shortly.");
            modal.style.display = 'none';
            pickupForm.reset();
        });
    }
});