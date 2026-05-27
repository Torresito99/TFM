pipeline {
    agent any

    // En un Multibranch, Jenkins hace la descarga del código solito en segundo plano antes de empezar.
    // Así que NO necesitamos el stage de "Descarga". Vamos directos a la acción:

    stages {
        stage('Compilacion') {
            steps {
                echo 'Compilando proyecto...'
                sh 'exit 0' 
            }
        }
        stage('Testeo') {
            steps {
                echo 'Lanzando tests...'
                // Cambia a "exit 1" para simular fallo
                sh 'exit 0'
            }
        }
    }
    
    post {
        always {
            script {
                // Obtener el Hash del Commit
                def commitHash = sh(script: 'git rev-parse HEAD', returnStdout: true).trim()
                
                // Obtener el Nombre del Autor del último commit usando git (así es universal)
                def authorName = sh(script: 'git log -1 --pretty=format:"%an"', returnStdout: true).trim()

                // Definir estados para la API
                def status = currentBuild.result == 'SUCCESS' ? 'success' : 'failure'
                
                // Construir la descripción personalizada incluyéndote a ti o a "María", "Pepe"...
                def description = currentBuild.result == 'SUCCESS' ? "Ok. Autor: ${authorName}" : "Fallo el codigo de: ${authorName}"
                
                withCredentials([string(credentialsId: 'token-github', variable: 'GITHUB_TOKEN')]) {
                    // AQUÍ DEBES CAMBIAR TuUsuario/TuRepositorio
                    sh """
                        curl -X POST -H "Authorization: token ${GITHUB_TOKEN}" \
                        -H "Accept: application/vnd.github.v3+json" \
                        https://api.github.com/repos/Torresito99/TFM/statuses/${commitHash} \
                        -d '{"state":"${status}","context":"Jenkins CI","description":"${description}"}'
                    """
                }
            }
        }
    }
}
