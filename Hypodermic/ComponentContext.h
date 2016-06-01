#pragma once

#include <memory>
#include <vector>

#include "Hypodermic/AutowireableConstructor.h"
#include "Hypodermic/ConstructorDescriptor.h"
#include "Hypodermic/IRegistration.h"
#include "Hypodermic/IRegistrationScope.h"
#include "Hypodermic/IResolutionContainer.h"
#include "Hypodermic/IRuntimeRegistrationBuilder.h"
#include "Hypodermic/IsComplete.h"
#include "Hypodermic/NestedRegistrationScope.h"
#include "Hypodermic/ResolutionContext.h"
#include "Hypodermic/TypeInfo.h"


namespace Hypodermic
{

    class Container;


    class ComponentContext
    {
    public:
        ComponentContext(const std::weak_ptr< Container >& container,
                         const std::shared_ptr< IRegistrationScope >& registrationScope,
                         const std::shared_ptr< IRuntimeRegistrationBuilder >& runtimeRegistrationBuilder)
            : m_registrationScope(registrationScope)
            , m_runtimeRegistrationBuilder(runtimeRegistrationBuilder)
            , m_resolutionContext(*this, container)
        {
        }

        /// <summary>
        /// Resolve an instance of type T
        /// </summary>
        /// <param name="T">The type to resolve (i.e. get an instance of T)</param>
        /// <returns>A shared pointer on an instance of type T</returns>
        template <class T>
        std::shared_ptr< T > resolve()
        {
            static_assert(Traits::IsComplete< T >::value, "T should be a complete type");

            auto&& instance = resolve< T >(createKeyForType< T >());
            if (instance != nullptr)
                return instance;

            return resolveIfTypeCanBeRegistered< T >();
        }

        /// <summary>
        /// Resolve all instances of type T
        /// </summary>
        /// <param name="T">The type to resolve (i.e. get all instances of T)</param>
        /// <returns>A vector of shared pointers on instances of type T</returns>
        template <class T>
        std::vector< std::shared_ptr< T > > resolveAll()
        {
            static_assert(Traits::IsComplete< T >::value, "T should be a complete type");

            return resolveAll< T >(createKeyForType< T >());
        }

    private:
        template <class T>
        std::shared_ptr< T > resolve(const TypeAliasKey& typeAliasKey)
        {
            std::vector< std::shared_ptr< RegistrationContext > > registrationContexts;
            if (!tryGetRegistrations(typeAliasKey, registrationContexts))
                return nullptr;

            if (registrationContexts.empty())
                return nullptr;

            return resolve< T >(typeAliasKey, registrationContexts.back());
        }

        template <class T>
        std::shared_ptr< T > resolve(const TypeAliasKey& typeAliasKey, const std::shared_ptr< RegistrationContext >& registrationContext)
        {
            auto& resolutionContainer = registrationContext->resolutionContainer();
            return std::static_pointer_cast< T >(resolutionContainer.getOrCreateComponent(typeAliasKey, registrationContext->registration(), m_resolutionContext));
        }

        template <class T>
        std::vector< std::shared_ptr< T > > resolveAll(const TypeAliasKey& typeAliasKey)
        {
            std::vector< std::shared_ptr< RegistrationContext > > registrationContexts;
            if (!tryGetRegistrations(typeAliasKey, registrationContexts))
                return std::vector< std::shared_ptr< T > >();

            return resolveAll< T >(typeAliasKey, registrationContexts);
        }

        template <class T>
        std::vector< std::shared_ptr< T > > resolveAll(const TypeAliasKey& typeAliasKey, const std::vector< std::shared_ptr< RegistrationContext > >& registrationContexts)
        {
            std::vector< std::shared_ptr< T > > instances;

            for (auto&& registrationContext : registrationContexts)
                instances.push_back(resolve< T >(typeAliasKey, registrationContext));

            return instances;
        }

        bool tryGetRegistrations(const TypeAliasKey& typeAliasKey, std::vector< std::shared_ptr< RegistrationContext > >& registrationContexts) const
        {
            return m_registrationScope->tryGetRegistrations(typeAliasKey, registrationContexts);
        }

        template <class T>
        std::shared_ptr< T > resolveIfTypeCanBeRegistered()
        {
            if (!tryToRegisterType< T >(*m_registrationScope, Traits::HasAutowireableConstructor< T >()))
                return nullptr;

            return resolve< T >(createKeyForType< T >());
        }

        template <class T>
        bool tryToRegisterType(IRegistrationScope& scope, std::true_type /* T has autowireable constructor */)
        {
            auto&& factory = Traits::ConstructorDescriptor< T >::describe();

            scope.addRegistration(m_runtimeRegistrationBuilder->build
            (
                Utils::getMetaTypeInfo< T >(),
                [factory](const IRegistration& r, IResolutionContext& c) { return std::static_pointer_cast< void >(factory(r, c)); }
            ));

            return true;
        }

        template <class>
        bool tryToRegisterType(IRegistrationScope&, std::false_type)
        {
            return false;
        }

    private:
        const std::shared_ptr< IRegistrationScope >& m_registrationScope;
        const std::shared_ptr< IRuntimeRegistrationBuilder >& m_runtimeRegistrationBuilder;
        ResolutionContext m_resolutionContext;
    };

} // namespace Hypodermic