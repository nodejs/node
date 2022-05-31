node('jdk11-mvn3.8.4') {
    stage('git') {
        git 'https://github.com/bhargavi-vaduguri/nodejs.git'
    }
    stage('build') {
        sh 'npm install'
    }
    stage('archive') {
        archive 'target/*.jar'
    }
} 